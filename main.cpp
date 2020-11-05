#include "Tuda.h" //соединения КЛИЕНТ - ПРОКСИ
#include "Suda.h" //соединения ПРОКСИ - СЕРВЕР
#include "Cache.h"
#include "HTTP.h"
#include "biblio.h"

bool LOG_CACHE = true; //логирование: из кэша или нет
bool LOG = true; //логирование: другие записи

Cache cache;
fd_set allset;
int maxfd;

int main(int argc, char **argv) {
    signal(SIGPIPE, SIG_IGN); //SIGPIPE посылается, когда сокет, в который я пишу, закрывается с другой стороны
    
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
    int flags = fcntl(listenfd, F_GETFL, 0);
    if (flags == -1) {
        printf("ERROR MAIN fcntl 1: why?\n");
        close(listenfd);
        return 0;
    }
    flags |= O_NONBLOCK;
    if (fcntl(listenfd, F_SETFL, flags) == -1) {
        printf("ERROR MAIN fcntl 2: why?\n");
        close(listenfd);
        return 0;
    }

    //ДЛЯ SELECT
    maxfd = listenfd;
    FD_ZERO(&allset); //это, а также FD_CLR, FD_ISSET, FD_SET - макросы
    FD_SET(listenfd, &allset);

    //СПИСОК СОЕДИНЕНИЙ: КЛИЕНТ - ПРОКСИ
    std::list <Tuda> tudas;

    //для очистки кэша. Запись актуальна какое-то время после её появления и хранится, пока есть хоть один связанный с ней Tuda
    int timerC = 0;

    for(;;) {
        //время от времени очищаем кэш
        if (timerC >= 100000) {
            cache.clear();
            timerC = 0;
            printf("1\n");
        }
        ++timerC;

        //select
        fd_set rset, wset;
        rset = wset = allset;
        FD_CLR(listenfd, &wset);
        int nready;
        while((nready = select(maxfd + 1, &rset, &wset, NULL, NULL) < 0)) {
            if (errno == ENOMEM) {
                tudas.pop_back();
            } else if ((errno != EINTR) && (errno != EWOULDBLOCK) && (errno != ECONNABORTED) && (errno != EPROTO)) {
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
              //  printf("1\n");
                tudas.push_back(Tuda(connfd, cliaddr));    
                FD_SET(connfd, &allset);
                //FD_SET(sockfd, &allset);
                if (maxfd < connfd) {
                    maxfd = connfd + 1;
                }
                if (--nready <= 0) {
                    continue;
                }
            }
        }

        //пробегаем все Tuda
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
                ++it;
            }
            if (nready == 0) break;
        }

        //пробегаем все Suda
        std :: map<std :: string, Suda*>::iterator itC = cache.m.begin(); 
        std :: map<std :: string, Suda*>::iterator endC = cache.m.end(); 
        while (itC != endC)
        {
            int serverSocket = (*itC).second -> serverSocket;
            bool opened = true;
            if (FD_ISSET(serverSocket, &rset)) {
               // printf("5\n");
                opened = (*itC).second -> readServerToProxy();
                --nready;
            }
            if (opened && FD_ISSET(serverSocket, &wset)) {
              //  printf("4\n");
                opened = (*itC).second -> writeProxyToServer();
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
