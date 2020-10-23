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

extern bool LOG; //делать логирование
extern bool LINE_BY_LINE; //это 26 лаба
extern int MAX_STR;

//эту структуру можно использовать только в каком-то контейнере
//(потому что, когда я её писала, мне взбрело в голову,
// что так её можно оптимизировать)
struct Pipe {
	int clientSocket;
	int serverSocket; //сокет туда, куда мы всё транслируем

	//данные считываем в буфер, потому что
	//может возникнуть ситуация, когда
	//мы считали из одного сокета много байт
	//а в другой столько не можем записать
	char *bufClientToServer;
	char *iptrClientToServer; //куда в буфере пишем
	char *optrClientToServer; //откуда в буфере читаем
	int stdineofClientToServer;

	char *bufServerToClient;
	char *iptrServerToClient; //куда в буфере пишем
	char *optrServerToClient; //откуда в буфере читаем	
	int stdineofServerToClient;

	char ip[16]; //для логов
	int port;  //тоже для логов
	int cntStr; //для 26 лабы

	static const int BUF_SIZE = 1024;

	static Pipe newPipe(int ClientSocket, int ServerSocket, sockaddr_in cliaddr){
		return Pipe(ClientSocket, ServerSocket, cliaddr);
	}

	static Pipe newPipe(int ClientSocket, int ServerSocket) {
		return Pipe(ClientSocket, ServerSocket);
	}

	Pipe& operator= (const Pipe &pipe) = delete; 

	//при копировании данные из буфера теряются, но они вроде и не нужны
	//и то, откуда скопировали не надо больше использовать
	Pipe(const Pipe &copy) {
		clientSocket = copy.clientSocket;
		serverSocket = copy.serverSocket;

		iptrClientToServer = optrClientToServer = bufClientToServer = new char[BUF_SIZE];
		iptrServerToClient = optrServerToClient = bufServerToClient = new char[BUF_SIZE];
		strcpy(ip, copy.ip); //использую Си-строки, потому что мне нужно только представление char[]
		port = copy.port;

		stdineofClientToServer = false;
		stdineofServerToClient = false;
		cntStr = 0; //лаба 26

        //чтобы не было двух pipe, работающих с одними сокетами
		Pipe& cp = const_cast<Pipe&>(copy);
		cp.clientSocket = -1;
		cp.serverSocket = -1;
	}

	~Pipe() {
		//close(clientSocket);
		//close(serverSocket);
		delete[] bufClientToServer;
		bufClientToServer = NULL;
		delete[] bufServerToClient;
		bufServerToClient = NULL;
	}

	bool isEmpty() {
		return clientSocket == -1;
	}

	//считаем, что pipe не пуст
	bool readClient() {
		int n = readP(clientSocket, serverSocket, bufClientToServer, &iptrClientToServer, &optrClientToServer, &stdineofClientToServer);
		if (LOG && n > 0) printf("client %s : %d - client -> proxy - %d bytes\n", ip, port, n);
		if (stdineofServerToClient == 2 || stdineofClientToServer == 2) {
			markEmpty();
			return true;
		}
		return false;
	}

	//считаем, что pipe не пуст
	bool readServer() {
		int n = readP(serverSocket, clientSocket, bufServerToClient, &iptrServerToClient, &optrServerToClient, &stdineofServerToClient);
		if (LOG && n > 0) printf("client %s : %d - server -> proxy - %d bytes\n", ip, port, n);
		if (stdineofServerToClient == 2 || stdineofClientToServer == 2) {
			markEmpty();
			return true;
		}
		return false;
	}

	//считаем, что pipe не пуст
	bool writeServer() { //?
		int n = writeP(clientSocket, serverSocket, bufClientToServer, &iptrClientToServer, &optrClientToServer, &stdineofClientToServer);
		if (LOG && n > 0) printf("client %s : %d - client <- proxy - %d bytes\n", ip, port, n);
		if (stdineofServerToClient == 2 || stdineofClientToServer == 2) {
			markEmpty();
			return true;
		}
		return false;
	}

	//считаем, что pipe не пуст
	bool writeClient() { //?
		int n = writeP(serverSocket, clientSocket, bufServerToClient, &iptrServerToClient, &optrServerToClient, &stdineofServerToClient);
		if (LOG && n > 0) printf("client %s : %d - server <- proxy - %d bytes\n", ip, port, n);
		if (stdineofServerToClient == 2 || stdineofClientToServer == 2) {
			markEmpty();
			return true;
		}
		return false;
	}	

private:
	Pipe(int pClientSocket, int pServerSocket, sockaddr_in cliaddr) : clientSocket(pClientSocket), serverSocket(pServerSocket), stdineofClientToServer(0), stdineofServerToClient(0) {
		iptrClientToServer = optrClientToServer = bufClientToServer = NULL;
		iptrServerToClient = optrServerToClient = bufServerToClient = NULL;
		inet_ntop(AF_INET, &cliaddr.sin_addr,ip,16);
		port = ntohs(cliaddr.sin_port);
		cntStr = 0; //лаба 26
	}

	Pipe(int pClientSocket, int pServerSocket) : clientSocket(pClientSocket), serverSocket(pServerSocket), stdineofClientToServer(0), stdineofServerToClient(0) {
		iptrClientToServer = optrClientToServer = bufClientToServer = NULL;
		iptrServerToClient = optrServerToClient = bufServerToClient = NULL;
		strcpy(ip, "");
		port = 0;
		cntStr = 0; //лаба 26
	}

	//считаем, что Pipe не пуст
	int readP(int socketToRead, int socketToWrite, char *buf, char **iptr, char **optr, int *stdineof) {
		int n = 0;
		int cnt = buf + BUF_SIZE - (*iptr);
		while ((n = read(socketToRead, *iptr, cnt)) < 0) {
			if (errno == EINTR) {											//если нас вырубили по сигналу, можно забить и сделать в след раз или вот так
				continue;											
			} else if (errno == EWOULDBLOCK) {
				break;
			} else {
				printf("ERROR PIPE 1: read\n");
				return 0; //что-то другое?
			} 
		}
		if (n == 0 && cnt > 0) { //0 байтов может быть считано в 2 случаях: EOF и нет места в буфере
			*stdineof = 1;
			if (*optr == *iptr) {
				shutdown(socketToWrite, SHUT_WR);
				*stdineof = 2;
			}
		} else if (n > 0) {
			*iptr += n;
		}
		return n;
	}

	//считаем, что Pipe не пуст
	int writeP(int socketToRead, int socketToWrite, char *buf, char **iptr, char **optr, int *stdineof) {
		int n = 0;
		int cnt = *iptr - *optr;
		int i;

		if (LINE_BY_LINE) {
			int cStr = cntStr;
			for (i = 0; i < cnt && cStr < MAX_STR; ++i) if ((*optr)[i] == '\n') ++cStr;
			if (cnt > i) cnt = i;
		}

		while (cnt > 0 && (n = write(socketToWrite, *optr, cnt)) < 0) {
			if (errno == EINTR) {										
				continue;											
			} else if (errno == EWOULDBLOCK) {
				break;
			} else {
				printf("ERROR PIPE 2: write\n");
				return 0; //что-то другое?
			} 
		}
		if (n >= 0) {
			if (LINE_BY_LINE) for (i = 0; i < n; ++i) if ((*optr)[i] == '\n') ++cntStr; //26 лаба
			*optr += n;
			if (*optr == *iptr) {
				*optr = *iptr = buf;
				if (*stdineof == 1) {
					shutdown(socketToWrite, SHUT_WR);
					*stdineof = 2;
				}
			}
		}
		return n;
	}

	void markEmpty() {
		close(clientSocket);
		close(serverSocket);
		if (LOG) printf("client %s : %d - client -> proxy - closed\n", ip, port);
		clientSocket = serverSocket = -1;
	}
};
