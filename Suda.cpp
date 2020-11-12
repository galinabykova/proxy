#include "Suda.h"

Suda::Suda(Request request) 
{
	//открываем соединение ПРОКСИ - СЕРВЕР
	req = request;
	//ip, port хоста
	struct hostent* h = gethostbyname(req.host.c_str());
	doOrNot(h == NULL, "ERROR SUDA 1: incorrect host");
	if (h == NULL) {serverSocket = -1; error = true; return;}				//надо бы сделать исключения, но нет
	//СОКЕТ
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	doOrNot(sockfd == -1, "ERROR SUDA 2: unable to create socket");
	if (sockfd == -1) {serverSocket = -1; error = true; return;}			//надо бы сделать исключения, но нет
	//разобраться с неблокирующим вводом-выводом
	int flags = fcntl(sockfd, F_GETFL, 0);
	if (flags == -1) {
		printf("ERROR SUDA fcntl 1: why?\n");
		close(serverSocket);
		serverSocket = -1;
		error = true;
		if (LOG) printf("%s %d closed\n", ip, port);
		return;
	}
	flags |= O_NONBLOCK;
	if (fcntl(sockfd, F_SETFL, flags) == -1) {
		printf("ERROR SUDA fcntl 2: why?\n");
		close(serverSocket);
		serverSocket = -1;
		error = true;
		if (LOG) printf("%s %d closed\n", ip, port);
		return;
	}
	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family =  AF_INET;
	servaddr.sin_port = htons(port); //

	inet_pton(AF_INET, inet_ntoa(*(struct in_addr*)h->h_addr), &servaddr.sin_addr);
	inet_ntop(AF_INET, &servaddr.sin_addr,ip,16);
	if (connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1) { //может быть прервано сигналом, но вроде бы даже в этом случае нельзя использовать после сокет (Стивенс, 130)
		if (errno != EINPROGRESS) {
			printf("ERROR SUDA connect: why??\n");
			close(serverSocket);
			serverSocket = -1;
			error = true;
			if (LOG) printf("%s %d closed\n", ip, port);
			return;
		} else {
			tryToConnectNow = true;
		}
	}
	serverSocket = sockfd;

	reply = Reply();
	buf = new char[MAX_CNT_IN_ONE_TIMES];
}

//то, откуда скопировали не надо больше использовать
Suda::Suda(const Suda &copy) 
{
	//printf("Suda\n");
	serverSocket = copy.serverSocket;
	req = copy.req;
	indexNext = copy.indexNext;
	reply = copy.reply;
	cntOfReaders = copy.cntOfReaders;
	buf = copy.buf;
	strcpy(ip, copy.ip); //использую Си-строки, потому что мне нужно только представление char[]
	port = copy.port;
	tryToConnectNow = copy.tryToConnectNow;

    //чтобы не было двух pipe, работающих с одними сокетами
	Suda& cp = const_cast<Suda&>(copy);
	cp.serverSocket = -1;
	cp.buf = NULL;
}

//Suda& operator= (const Suda &suda) = delete; 

Suda::~Suda() 
{
	//printf("Suda\n");
	if (serverSocket != -1) {
		close(serverSocket);
		if (LOG) printf("%s %dclosed\n", ip, port);
	}
	serverSocket = -1;
	delete[] buf;
	buf = NULL;
}

//в многопоточном как-нить защитить функции с cntOfReaders
void Suda::incCntOfReaders() 
{
	//printf("Suda\n");
	++cntOfReaders;
}
void Suda::deqCntOfReaders() 
{
	//printf("Suda\n");
	--cntOfReaders;
}
int Suda::getCntOfReaders() 
{
	//printf("Suda\n");
	return cntOfReaders;
}

bool Suda::readServerToProxy() 
{
	if (error) return false;
	if (tryToConnectNow) {
		error = true;
		close(serverSocket);
		serverSocket = -1;
		if (LOG) printf("%s : %d not connected\n", ip, port);
		if (LOG) printf("%s %dclosed\n", ip, port);
		return false;
	}
	int n = 0;
	while ((n = read(serverSocket, buf, MAX_CNT_IN_ONE_TIMES)) < 0) 
	{					
		if (errno == EINTR) {//если нас вырубили по сигналу, можно забить и сделать в след раз или вот так
			continue;											
		} else if (errno == EWOULDBLOCK || errno == EAGAIN) {
			break;
		} else {
			printf("ERROR SUDA 1: read\n");
			printf("%s\n", strerror(errno));
			error = true;
			close(serverSocket);
			serverSocket = -1;
			if (LOG) printf("%s %d closed\n", ip, port);
			return false; //что-то другое?
		} 
	}
	if (n > 0) {
		reply.add(buf, n);
		if (LOG && (n > 0)) printf("server %s : %d -> proxy - %d bytes\n", ip, port, n);
		if (mime == "" && code == 0) {
			mime = reply.mime;
			code = reply.code;
		}
	} 
	if (reply.isEndToRead) {
		close(serverSocket);
		serverSocket = -1;
		if (LOG) printf("%s %d closed\n", ip, port);
/*
		int size = reply.v.size();
		for (int i=0; i<size; ++i) {
	 		std::cout << reply.v[i];
		}
*/
		//suda = cache.sudaFor(reqBuffer); создаст новую или вернёт существующую
		return false;
	}
	/*if (n == 0) { //если считано 0 для select, значит достигнут EOF, странно, если мы дошли до сюда, а не стопорнулись раньше
		return false;
	}*/
	return true;
}

bool Suda::writeProxyToServer() 
{
	if (error) return false;
	if (tryToConnectNow) tryToConnectNow = false;
	int n = 0;
	int cnt = req.v.size() - indexNext; //suda.lastIndex - index;  что-то в этом роде
	char* ptr = &(req.v[indexNext]); //можно, т. к. не должно меняться (т.к нет персистентных соединений)
	while (cnt > 0 && (n = write(serverSocket, ptr, cnt)) < 0) 
	{
		if (errno == EINTR) {										
			continue;											
		} else if (errno == EWOULDBLOCK || errno == EAGAIN) {
			break;
		} else {
			printf("ERROR SUDA 2: write\n");
			printf("%s\n", strerror(errno));
			errno = true;
			close(serverSocket);
			serverSocket = -1;
			if (LOG) printf("%s %d closed\n", ip, port);
			return false; //что-то другое?
		} 
	}
	if (n > 0) {
		/*for (int i=0; i<n; ++i) {
			printf("%c", ptr[i]);
		}*/
		//printf("\n");
		indexNext +=n;
		if (LOG && (n > 0)) printf("proxy -> server %s : %d - %d bytes\n", ip, port, n);
	}
	return true;
}

bool Suda::isEnd(int clientIndex) {
	//printf("Suda\n");
	return reply.isEndToRead && clientIndex >= reply.v.size();
}
