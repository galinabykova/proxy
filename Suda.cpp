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
#include "HTTP.cpp"
extern bool LOG; //делать логирование

//возможно, добавить что-то вроде флага ошибки
//добавить оператор сравнения
struct Suda {
	//нужен для кэширования, отличает Suda друг от друга
	ReqBuffer request;
	int indexNext = 0;

	int serverSocket; //сокет туда, куда мы всё транслируем

	//АККУРАТНО read использует char*, a &v[0] предполагает, что мы ничего не добавляем в вектор между &v[0] и использованием)
	//для получения ответа
	ReqBuffer reply; //нужен, поскольку нужно хранить весь запрос в одно время, но непонятен его размер
	static const int MAX_CNT_IN_ONE_TIMES = 1024;
	char* buf; //нужен, поскольку с ним работает read

	int timeOfLastReading = 0; //для таймаута

	char ip[16]; //для логов
	int port;  //тоже для логов

	bool error = false;
	std :: string mime = "";
	int code = 0;
	int time = 0;

	private :
	int cntOfReaders = 0; //количество читающих сейчас

	public:
	Suda(ReqBuffer reqBuffer) {
		request = reqBuffer;
		Request req = Request(reqBuffer);
		port = req.port;
		struct hostent* h = gethostbyname(req.host.c_str());
		doOrNot(h == NULL, "ERROR SUDA 1: incorrect host");
		if (h == NULL) {error = true; return;}				//надо бы сделать исключения, но нет

		//СОКЕТ
		int sockfd = socket(AF_INET, SOCK_STREAM, 0);
		doOrNot(sockfd == -1, "ERROR SUDA 2: unable to create socket");
		if (sockfd == -1) {error = true; return;}				//придумай, что делать в таких ситуациях

		struct sockaddr_in servaddr;
		memset(&servaddr, 0, sizeof(servaddr));
		servaddr.sin_family =  AF_INET;
		servaddr.sin_port = htons(port); //

		inet_pton(AF_INET, inet_ntoa(*(struct in_addr*)h->h_addr), &servaddr.sin_addr);
		inet_ntop(AF_INET, &servaddr.sin_addr,ip,16);
		if (connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1) { //может быть прервано сигналом, но вроде бы даже в этом случае нельзя использовать после сокет (Стивенс, 130)
			while (close(sockfd) == -1) if (errno != EINTR) break;		
			doOrNot(true, "ERROR SUDA 3: why???");
			return; //придумай, что делать в таких ситуациях
		}
		serverSocket = sockfd;

		reply = ReqBuffer();
		buf = new char[MAX_CNT_IN_ONE_TIMES];
	}

	//то, откуда скопировали не надо больше использовать
	Suda(const Suda &copy) {
		serverSocket = copy.serverSocket;
		request = copy.request;
		indexNext = copy.indexNext;
		reply = copy.reply;
		cntOfReaders = copy.cntOfReaders;
		timeOfLastReading = copy.timeOfLastReading;
		buf = copy.buf;
		strcpy(ip, copy.ip); //использую Си-строки, потому что мне нужно только представление char[]
		port = copy.port;

        //чтобы не было двух pipe, работающих с одними сокетами
		Suda& cp = const_cast<Suda&>(copy);
		cp.serverSocket = -1;
		cp.buf = NULL;
	}

	//Suda& operator= (const Suda &suda) = delete; 

	~Suda() {
		if (serverSocket != -1) {
			close(serverSocket);
		}
		if (serverSocket != -1) {
			close(serverSocket);
		}
		delete[] buf;
		buf = NULL;
	}

	//в многопоточном как-нить защитить функции с cntOfReaders
	void incCntOfReaders() {
		++cntOfReaders;
	}
	void deqCntOfReaders() {
		--cntOfReaders;
	}
	int getCntOfReaders() {
		return cntOfReaders;
	}

	//считаем, что Pipe не пуст
	bool readServerToProxy() {
		int n = 0;
		while ((n = read(serverSocket, buf, MAX_CNT_IN_ONE_TIMES)) < 0) {					
			if (errno == EINTR) {											//если нас вырубили по сигналу, можно забить и сделать в след раз или вот так
				continue;											
			} else if (errno == EWOULDBLOCK) {
				break;
			} else {
				printf("ERROR SUDA 1: read\n");
				return false; //что-то другое?
			} 
		}
		if (n > 0) {
			reply.add(buf, n);
			if (LOG && (n > 0)) printf("server %s : %d -> proxy - %d bytes\n", ip, port, n);
			if (mime == "") {
				Reply r = Reply(reply);
				mime = r.mime;
				code = r.code;
			}
		} 
		if (reply.isEndToRead) {
			//раскоментить и проверить, когда сделаешь SUDA
			/*Reply replyR = Reply(reply);
			int size = replyR.request.v.size();
			for (int i=0; i<size; ++i) {
		 		std :: cout << replyR.request.v[i];
			}*/

			//suda = cache.sudaFor(reqBuffer); создаст новую или вернёт существующую
			return true;
		}
		if (n == 0) { //если считано 0 для select, значит достигнет EOF, странно, если мы дошли до сюда, а не стопорнулись раньше
			return false;
		}
		return true;;
	}

	//ложь, если сервер нас послал
	bool writeProxyToServer() {
		int n = 0;
		int cnt = request.v.size() - indexNext; //suda.lastIndex - index;  что-то в этом роде
		char* ptr = &(request.v[0]); //можно, т. к. не должно меняться (т.к нет персистентных соединений)
		while (cnt > 0 && (n = write(serverSocket, ptr + indexNext, cnt)) < 0) {
			if (errno == EINTR) {										
				continue;											
			} else if (errno == EWOULDBLOCK) {
				break;
			} else {
				printf("ERROR SUDA 2: write\n");
				return false; //что-то другое?
			} 
		}
		if (n > 0) {
			indexNext +=n;
			if (LOG && (n > 0)) printf("proxy -> server %s : %d - %d bytes\n", ip, port, n);
		}
		return true;
	}

	bool isEnd(int clientIndex) {
		return reply.isEndToRead && clientIndex == reply.v.size();
	}
};
