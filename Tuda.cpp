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
#include <vector>
#include "Cache.cpp"

bool LOG = false; //делать логирование

Cache cache;
fd_set allset;
int maxfd;

//ВСЁ ЛИ НОРМ Со ССЫЛКАМИ СЮДА
//эту структуру можно использовать только в каком-то контейнере
//(потому что, когда я её писала, мне взбрело в голову,
// что так её можно оптимизировать)
struct Tuda {
	int clientSocket;

	//для получения GET запроса
	ReqBuffer reqBuffer; //нужен, поскольку нужно хранить весь запрос в одно время, но непонятен его размер
	static const int MAX_CNT_IN_ONE_TIMES = 1024;
	char* buf; //нужен, поскольку с ним работает read

	//для записи
	Suda* suda = NULL;
	int index; //индекс байта, который нужно записать следующим из SUDA

	char ip[16]; //для логов
	int port;  //тоже для логов

	Tuda(int clSocket, sockaddr_in cliaddr) {
		clientSocket = clSocket;
		reqBuffer = ReqBuffer();
		buf = new char[MAX_CNT_IN_ONE_TIMES];
		index = 0;
		inet_ntop(AF_INET, &cliaddr.sin_addr,ip,16);
		port = ntohs(cliaddr.sin_port);
	}

	//при копировании данные из буфера теряются, но они вроде и не нужны
	//и то, откуда скопировали не надо больше использовать
	Tuda(const Tuda &copy) {
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

	Tuda& operator= (const Tuda &tuda) = delete; 

	//возвращает ложь, если это соединение не действительно
	bool readClientToProxy() {
		int n = 0;
		while ((n = read(clientSocket, buf, MAX_CNT_IN_ONE_TIMES)) < 0) {					
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
				//раскоментить и проверить, когда сделаешь SUDA
				/*Request request = Request(reqBuffer);
				int size = request.request.v.size();
				for (int i=0; i<size; ++i) {
		 			std :: cout << request.request.v[i];
				}*/
				//НЕВЕРНО УДАЛЯЕТСЯ СЛИШКОМ РАНО  (ПОКА)
				//пока без кэша
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

	//ложь, если что-то пошло не так
	bool writeProxyToClient() {
		if (suda == NULL) return true;
		int n = 0;
		if (suda->error) {
			return false;
		}
		int cnt = suda->reply.v.size() - index;
		//НУЖНА СИНХРОНИЗАЦИЯ
		char* ptr = &(suda->reply.v[0]); //suda->ptr индекс на вектор? что-то вроде &v[0], но осторожно, действителен до следующего добавления в вектор
		while (cnt > 0 && (n = write(clientSocket, ptr, cnt)) < 0) {
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
		//потом как-то удалять тех, у кого clientSocket == -1
		if (suda->isEnd(index)) {
			close(clientSocket);
			suda->deqCntOfReaders();
			return false;
		}
		return true;
	}

	~Tuda() {
		delete[] buf;
		if (clientSocket != -1) {
			close(clientSocket);
		}
		//уменьшить счётчик suda
	}
};

int main(int argc, char **argv) {
  //РАЗБОР АРГУМЕНТОВ КОМАНДНОЙ СТРОКИ
    doOrDie(argc < 2, "ERROR1: param <my port>");
    int MY_PORT = atoi(argv[1]);
    doOrDie(MY_PORT == 0, "ERROR2: incorrect <my port>"); //0 - некорректное значение, так как в этом случае порт будет выбран динамически

    //ДЛЯ ПРОСЛУШИВАЕМОГО СОКЕТА
    int listenfd = socket(AF_INET, SOCK_STREAM, 0); //IPv4, потоковый сокет, протокол - по умолчанию для данного семейства и типа
    doOrDie(listenfd == -1, "ERROR5: unable to create socket");
    struct sockaddr_in servaddr; //структура для адреса сокета IPv4
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); //выбираем любой IP
                                                  //htonl - переводит в сетевой порядок байт
    servaddr.sin_port = htons(MY_PORT);
    int bv = bind(listenfd, (struct sockaddr*) &servaddr, sizeof(servaddr)); //связываем адрес и сокет, если не сделать этого, будет выбран автоматически
                                                                    //sockaddr - универсальная структура адреса сокета, нужно преобразовать к ней перед передачей в функцию
    doOrDie(bv < 0, "ERROR6: unable to bind socket. Maybe, you should use another <port>");
    bv = listen(listenfd, 10); //преобразует в пассивный сокет, backlog - мах число соединений, которое ядро может помещать в очереди
    doOrDie(bv < 0, "ERROR7: unable to listen socket. Why? This is really strange");

    //ДЛЯ SELECT
    maxfd = listenfd;
    FD_ZERO(&allset); //это, а также FD_CLR, FD_ISSET, FD_SET - макросы
    FD_SET(listenfd, &allset);

    //СПИСОК СОЕДИНЕНИЙ: КЛИЕНТ - ПРОКСИ
    std::list <Tuda> tudas;

    //для очистки кэша
    int timerC = 0;

    for(;;) {
        if (timerC >= 10000) {
            cache.clear();
            timerC = 0;
        }
        ++timerC;

        fd_set rset, wset;
        rset = wset = allset;
        FD_CLR(listenfd, &wset);
        int nready;
        while((nready = select(maxfd + 1, &rset, &wset, NULL, NULL) < 0)) {
            if (errno == ENOMEM) {
                tudas.pop_back();
            } else if (errno != EINTR) {
                printf("ERROR11: select. Why?\n");
                exit(0);
            }
        }

        if (FD_ISSET(listenfd, &rset)) {
            //ДЛЯ ПРИСОЕДИНЁННОГО СОКЕТА КЛИЕНТ - ПРОКСИ
            struct sockaddr_in cliaddr;
            unsigned int clilen = sizeof(cliaddr);
            int connfd = accept(listenfd, (struct sockaddr*) &cliaddr, &clilen);
            doOrNot(connfd < 0, "ERROR8: unable to connect to new client");

            //проверяем, можем ли добавлять ещё одного клиента
            if (tudas.size() + 1 == FD_SETSIZE) {
                printf("ERROR10: i haven't memory for new client\n");
                close(connfd);
            } else {
                //ДЛЯ СОКЕТА ПРОКСИ - СЕРВЕР ДЛЯ ЭТОГО КЛИЕНТА
                /*
                int sockfd = socket(AF_INET, SOCK_STREAM, 0);
                doOrdie(sockfd == -1, "ERROR9: unable to create socket");
                memset(&servaddr, 0, sizeof(servaddr));
                servaddr.sin_family = AF_INET;
                servaddr.sin_port = htons(PORT);
                inet_pton(AF_INET, IP, &servaddr.sin_addr); //проверили корректность IP в начале

                if (connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1) { //может быть прервано сигналом, но вроде бы даже в этом случае нельзя использовать после сокет (Стивенс, 130)
                    while (close(connfd) == -1) if (errno != EINTR) break;
                    while (close(sockfd) == -1) if (errno != EINTR) break;              
                } else {*/
                    tudas.push_back(Tuda(connfd, cliaddr));    
                    FD_SET(connfd, &allset);
                    //FD_SET(sockfd, &allset);
                    if (maxfd < connfd) {
                        maxfd = connfd + 1;
                    }
                    if (--nready <= 0) {
                        continue;
                    }
                //}
            }
        }

        std::list<Tuda>::iterator it = tudas.begin(); 
		std::list<Tuda>::iterator end = tudas.end();
        while (it != end)
        {
            int clientSocket = (*it).clientSocket;
            bool opened = true;
            if (FD_ISSET(clientSocket, &rset)) {
                opened = (*it).readClientToProxy();
                --nready;
            }
            if (opened && FD_ISSET(clientSocket, &wset)) {
                opened = (*it).writeProxyToClient();
                --nready;
            }
            if (!opened) {
                FD_CLR(clientSocket,&allset);
                it = tudas.erase(it);
            } else {
                ++it; // и переходим к следующему элементу
            }
            if (nready == 0) break;
        }

        std :: map<std :: string, Suda*>::iterator itC = cache.m.begin(); 
        std :: map<std :: string, Suda*>::iterator endC = cache.m.end(); 
        while (itC != endC)
        {
            int serverSocket = (*itC).second -> serverSocket;
            bool opened = true;;
            if (FD_ISSET(serverSocket, &wset)) {
                opened = (*itC).second -> writeProxyToServer();
                --nready;
            }
            if (opened && FD_ISSET(serverSocket, &rset)) {
                opened = (*itC).second -> readServerToProxy();
                --nready;
            }
            if (!opened) {
                FD_CLR(serverSocket,&allset);
            }
            ++itC;
            if (nready == 0) break;
        }
    }
}

