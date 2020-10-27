#include "Tuda.h"

Tuda::Tuda(int clSocket, sockaddr_in cliaddr) 
{
	clientSocket = clSocket;
	reqBuffer = ReqBuffer();
	buf = new char[MAX_CNT_IN_ONE_TIMES];
	index = 0;
	inet_ntop(AF_INET, &cliaddr.sin_addr,ip,16);
	port = ntohs(cliaddr.sin_port);
}

//при копировании данные из буфера теряются, но они вроде и не нужны
//и то, откуда скопировали не надо больше использовать
Tuda::Tuda(const Tuda &copy) 
{
	clientSocket = copy.clientSocket;
	reqBuffer = copy.reqBuffer;
	buf = copy.buf;
	index = copy.index;
	strcpy(ip, copy.ip); //использую Си-строки, потому что мне нужно только представление char[]
	port = copy.port;

    //чтобы не было двух tuda, работающих с одними сокетами
	Tuda& cp = const_cast<Tuda&>(copy);
	cp.clientSocket = -1;
	cp.buf = NULL;
}

//возвращает ложь, если что-то пошло не так
bool Tuda::readClientToProxy() 
{
	int n = 0;
	while ((n = read(clientSocket, buf, MAX_CNT_IN_ONE_TIMES)) < 0) 
    {					
		if (errno == EINTR) {											//если нас вырубили по сигналу, можно забить и сделать в след раз или вот так
			continue;											
		} else if (errno == EWOULDBLOCK) {
			break;
		} else {
			printf("ERROR TUDA 1: read\n");
			return false; //что-то другое?
		} 
	}
	if (n > 0) {
		reqBuffer.add(buf, n);
		if (LOG && (n > 0)) printf("client %s : %d -> proxy - %d bytes\n", ip, port, n);
		if (reqBuffer.isEndToRead) {
			/*Request request = Request(reqBuffer);
			int size = request.request.v.size();
			for (int i=0; i<size; ++i) {
	 			std :: cout << request.request.v[i];
			}*/
			suda = cache.add(reqBuffer);
			if (!(suda -> reply).isEndToRead) {
				int serverSocket = suda -> serverSocket;
				FD_SET(serverSocket, &allset);
				if (maxfd < serverSocket) {
                       maxfd = serverSocket + 1;
                   }
			}
			suda->incCntOfReaders();
			return true;
        }
	} 
	if (n == 0) { //если считано 0 для select, значит достигнет EOF, странно, если мы дошли до сюда, а не стопорнулись раньше
		return false;
	}
	return true;
}

//ложь, если что-то пошло не так или считали всё
bool Tuda::writeProxyToClient() 
{
	if (suda == NULL) return true;
	int n = 0;
	if (suda->error) {
		return false;
	}
	int cnt = suda->reply.v.size() - index;
	//НУЖНА СИНХРОНИЗАЦИЯ
	char* ptr = &(suda->reply.v[0]); //suda->ptr индекс на вектор? что-то вроде &v[0], но осторожно, действителен до следующего добавления в вектор
	while (cnt > 0 && (n = write(clientSocket, ptr, cnt)) < 0) 
    {
		if (errno == EINTR) {										
			continue;											
		} else if (errno == EWOULDBLOCK) {
			break;
		} else {
			printf("ERROR TUDA 2: write\n");
			return false; //что-то другое?
		} 
	}
	if (n > 0) {
		index +=n;
		if (LOG && (n > 0)) printf("proxy -> client %s : %d - %d bytes\n", ip, port, n);
	}
	if (suda->isEnd(index)) {
		close(clientSocket);
		suda->deqCntOfReaders();
        suda = NULL;
		return false;
	}
	return true;
}

Tuda::~Tuda() 
{
	delete[] buf;
	if (clientSocket != -1) {
		close(clientSocket);
	}
}



