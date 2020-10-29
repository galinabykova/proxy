#include <stdio.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <math.h>
#include <fcntl.h>
#include <errno.h>
#include <list>
#include <netdb.h>
#include <termios.h>
#include <string>
#include <iostream>
#include "Pipe.cpp"

bool LOG = false;
bool LINE_BY_LINE = true;
int MAX_STR = 25;

struct termios old,newT;

void doOrDie(bool condition, const char* message) {
	if (condition) {
		printf("%s\n",message);
		tcsetattr(0,TCSANOW,&newT);
		exit(0);
	}
}

struct MyURL {
	std :: string host;
	int port;
	std :: string request;

	MyURL(const char* URLchar) {
		/*
		 * URL - <схема>:[//[<логин>[:<пароль>]@]<хост>[:<порт>]][/<URL‐путь>][?<параметры>][#<якорь>]
		 * request GET [/<URL‐путь>][?<параметры>][#<якорь>] HTTP/1.0
		*/
		try {
			std :: string URL = URLchar;
			doOrDie(URL.find("http:", 0) != 0, "ERROR URL 1: not http");
			if (URL.find("http:", 0) != 0) {
				printf("ERROR URL 1: not http\n");
				host = ""; port = 0; request = "";
				return;
			}
			std :: string withoutScheme = URL.substr(5, URL.length());
			std :: string hostAndAfter;
			size_t pos;
			if ((pos = withoutScheme.find('@')) != std :: string :: npos) {
				hostAndAfter = withoutScheme.substr(pos+1, withoutScheme.length());
			} else {
				hostAndAfter = withoutScheme.substr(2, withoutScheme.length());
			}
			std :: string hostAndPort = hostAndAfter.substr(0, hostAndAfter.find('/'));

			host = hostAndPort.substr(0, hostAndPort.find(':'));

			if ((pos = hostAndPort.find(':')) != std :: string :: npos) {
				port = stoi(hostAndPort.substr(hostAndPort.find(':')+1, hostAndPort.length())); //может быть исключение
			} else {
				port = 80;
			}

			request = "GET " + hostAndAfter.substr(hostAndAfter.find('/'), hostAndAfter.length()) + " HTTP/1.0\r\n" +
			"Host: " + host + "\r\n" +
			"\r\n";
			std::cout << request << std:: endl;
		} catch (std :: out_of_range a) {
			printf("ERROR URL 2: incorrect URL\n");
			host = ""; port = 0; request = "";
			return;
		} catch (std :: invalid_argument a) {
			printf("ERROR URL 3: incorrect port\n");
			host = ""; port = 0;request = "";
			return;
		}
	}
};

int main (int argc, char **argv) {
	tcgetattr(0,&old);
	tcgetattr(0,&newT);
  	newT.c_lflag&=~ICANON;
  	tcsetattr(0,TCSANOW,&newT);
	doOrDie(argc < 2, "ERROR1: param <url>");

	MyURL myURL = argv[1];

	struct hostent* h = gethostbyname(myURL.host.c_str());
	doOrDie(h == NULL, "ERROR2: incorrect URL");

	//СОКЕТ
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	doOrDie(sockfd == -1, "ERROR9: unable to create socket");
	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family =  AF_INET;
	servaddr.sin_port = htons(myURL.port); //
	printf("%s\n",inet_ntoa(*(struct in_addr*)h->h_addr));
	inet_pton(AF_INET, inet_ntoa(*(struct in_addr*)h->h_addr), &servaddr.sin_addr);
	if (connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1) { //может быть прервано сигналом, но вроде бы даже в этом случае нельзя использовать после сокет (Стивенс, 130)
		while (close(sockfd) == -1) if (errno != EINTR) break;		
		tcsetattr(0,TCSANOW,&newT);
		exit(0);
	}

	//ПЕРЕДАЧА ЗАПРОСА
	int n;
	while ((n = write(sockfd, myURL.request.c_str(), myURL.request.length() + 1)) < 0) {
		if (errno == EINTR || errno == EWOULDBLOCK) {
			continue;											
		} else {
			printf("ERROR16\n");
			return 0; //что-то другое?
		} 
	}

	//ВЫВОД ОТВЕТА И ВСЁ ТАКОЕ

	//ДЛЯ SELECT
	int maxfd = sockfd;
	fd_set allsetR, allsetW;
	FD_ZERO(&allsetR);
	FD_ZERO(&allsetW); 
	FD_SET(sockfd, &allsetR);
	FD_SET(STDOUT_FILENO, &allsetW);

	//с Pipe надо работать с списке
	std::list <Pipe> pipes;
	pipes.push_back(Pipe::newPipe(sockfd, STDIN_FILENO));

	bool closed = false;
	while(!closed) {
		if (pipes.front().cntStr == MAX_STR) {
			FD_CLR(STDOUT_FILENO, &allsetW);
			FD_SET(STDIN_FILENO, &allsetR);
			printf("Press space to scroll down\n");
		}

		fd_set rset = allsetR;
		fd_set wset = allsetW;
		while(select(maxfd + 1, &rset, &wset, NULL, NULL) < 0) {
			if (errno != EINTR) {
				printf("ERROR11: select. Why?\n");
				tcsetattr(0,TCSANOW,&newT);
				exit(0);
			}
		}
		if (FD_ISSET(sockfd, &rset)) {
			closed = pipes.front().readClient();
		}
		if (!closed && FD_ISSET(STDOUT_FILENO, &wset)) {
			closed = pipes.front().writeServer();
		}
		
        if (pipes.front().cntStr == MAX_STR && FD_ISSET(STDIN_FILENO, &allsetR)) {
        	int n;
        	char b[3];
        	while ((n = read(STDIN_FILENO, b, 3)) < 0) {
				if (errno == EINTR) {											
					continue;											
				} else if (errno == EWOULDBLOCK) {
					break;
				} else {
					printf("ERROR\n");
					return 0; //что-то другое?
				} 
			}
			if (n > 0) pipes.front().cntStr = 0;
			FD_CLR(STDIN_FILENO, &allsetR);
			FD_SET(STDOUT_FILENO, &allsetW);
        }
    }
    tcsetattr(0,TCSANOW,&newT);
}
