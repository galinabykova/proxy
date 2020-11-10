#include "Tuda.h"

Tuda::Tuda(int clSocket, sockaddr_in cliaddr) 
{
	clientSocket = clSocket;
	int flags = fcntl(clientSocket, F_GETFL, 0);
	if (flags == -1) {
		printf("ERROR TUDA fcntl 1: why?\n");
		error = true;
		return;
	}
	flags |= O_NONBLOCK;
	if (fcntl(clientSocket, F_SETFL, flags) == -1) {
		printf("ERROR TUDA fcntl 2: why?\n");
		error = true;
		return;
	}
	req = Request();
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
	req = copy.req;
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
	if (error) {
		close(clientSocket);
		clientSocket = -1;
		if (LOG) printf("client %s : %d closed\n", ip, port);
		return false;
	}
	int n = 0;
	while ((n = read(clientSocket, buf, MAX_CNT_IN_ONE_TIMES)) < 0) 
    {					
		if (errno == EINTR) {											//если нас вырубили по сигналу, можно забить и сделать в след раз или вот так
			continue;											
		} else if (errno == EWOULDBLOCK || errno == EAGAIN) {
			break;
		} else {
			printf("ERROR TUDA 1: read\n");
			error = true;
			close(clientSocket);
			clientSocket = -1;
			if (LOG) printf("client %s : %d closed\n", ip, port);
			return false; //что-то другое?
		} 
	}
	if (n > 0) {
		req.add(buf, n);
		if (LOG && (n > 0)) printf("client %s : %d -> proxy - %d bytes\n", ip, port, n);
		if (req.isEndToRead) {
/*
			int size = req.v.size();
			for (int i=0; i<size; ++i) {
	 			std :: cout << req.v[i];
			}
*/
			suda = cache.add(req);
			suda -> incCntOfReaders();
			if (!(suda -> reply).isEndToRead) {
				int serverSocket = suda -> serverSocket;
				FD_SET(serverSocket, &allset);
				if (maxfd < serverSocket) {
                    maxfd = serverSocket + 1;
                }
			}
			return true;
        }
	} 
	/*
	if (n == 0) { //если считано 0 для select, значит достигнет EOF, странно, если мы дошли до сюда, а не стопорнулись раньше
		return true; //но это ведь на чтение
	}
	*/
	return true;
}

//ложь, если что-то пошло не так или считали всё
bool Tuda::writeProxyToClient() 
{
	if (error) {
		close(clientSocket);
		clientSocket = -1;
		if (LOG) printf("client %s : %d closed\n", ip, port);
		return false;
	}
	if (suda == NULL) return true;
	if (suda -> code == 0) return true;
	if (suda -> error) {
		close(clientSocket);
		clientSocket = -1;
		suda->deqCntOfReaders();
		/*suda = cache.add(reqBuffer);
		suda -> incCntOfReaders();
		if (!(suda -> reply).isEndToRead) {
			int serverSocket = suda -> serverSocket;
			FD_SET(serverSocket, &allset);
			if (maxfd < serverSocket) {
                maxfd = serverSocket + 1;
            }
		}*/
		if (LOG) printf("client %s : %d closed\n", ip, port);
		return false;
	}
	int n = 0;
	int cnt = suda->reply.v.size() - index;
	//НУЖНА СИНХРОНИЗАЦИЯ
	if (cnt <= 0) return true;
	char* ptr = &(suda->reply.v[index]); //suda->ptr индекс на вектор? что-то вроде &v[0], но осторожно, действителен до следующего добавления в вектор
	while ((n = write(clientSocket, ptr, cnt)) < 0) 
    {
		if (errno == EINTR) {										
			continue;											
		} else if (errno == EWOULDBLOCK || errno == EAGAIN) {
			break;
		} else {
			printf("ERROR TUDA 2: write\n");
			close(clientSocket);
			clientSocket = -1;
			if (LOG) printf("client %s : %d closed\n", ip, port);
			return false; //что-то другое?
		} 
	}
	if (n > 0) {
		index += n;
		if (LOG && (n > 0)) printf("proxy -> client %s : %d - %d bytes\n", ip, port, n);
	}
	if (suda -> isEnd(index)) {
		suda->deqCntOfReaders();
        suda = NULL;
        close(clientSocket);
		clientSocket = -1;
		if (LOG) printf("client %s : %d closed\n", ip, port);
		return false;
	}
	return true;
}

Tuda::~Tuda() 
{
	printf("aaaaaaaa\n");
	delete[] buf;
	printf("bbbbbbbbbbbb\n");
	buf = NULL;
	if (clientSocket != -1) {
		close(clientSocket);
		if (LOG) printf("client %s : %d closed\n", ip, port);
	}
}



