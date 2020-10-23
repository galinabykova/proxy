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
#include "Pipe.cpp"

int MY_PORT, PORT;
const char *IP;
const int MAX_COUNT_CLIENT = 510;

bool LOG = true;
bool LINE_BY_LINE = false;
int MAX_STR = 0;

void doOrdie(bool condition, const char* message) {
    if (condition) {
        printf("%s\n",message);
        exit(0);
    }
}

void doOrNot(bool condition, const char* message) {
    if (condition) {
        printf("%s\n",message);
    }
}

int main(int argc, char **argv) {
    //������ ���������� ��������� ������
    doOrdie(argc != 4, "ERROR1: param <my port> <ip> <port>");

    MY_PORT = atoi(argv[1]);
    doOrdie(MY_PORT == 0, "ERROR2: incorrect <my port>"); //0 - ������������ ��������, ��� ��� � ���� ������ ���� ����� ������ �����������

    IP = argv[2];
    in_addr_t sin_addr;
    doOrdie(inet_pton(AF_INET, IP, &sin_addr) == 0, "ERROR3: incorrect <ip>");
    
    PORT = atoi(argv[3]);
    doOrdie(PORT == 0, "ERROR4: incorrect <port>"); 

    //��� ��������������� ������
    int listenfd = socket(AF_INET, SOCK_STREAM, 0); //IPv4, ��������� �����, �������� - �� ��������� ��� ������� ��������� � ����
    doOrdie(listenfd == -1, "ERROR5: unable to create socket");
    struct sockaddr_in servaddr; //��������� ��� ������ ������ IPv4
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); //�������� ����� IP
                                                  //htonl - ��������� � ������� ������� ����
    servaddr.sin_port = htons(MY_PORT);
    int bv = bind(listenfd, (struct sockaddr*) &servaddr, sizeof(servaddr)); //��������� ����� � �����, ���� �� ������� �����, ����� ������ �������������
                                                                    //sockaddr - ������������� ��������� ������ ������, ����� ������������� � ��� ����� ��������� � �������
    doOrdie(bv < 0, "ERROR6: unable to bind socket. Maybe, you should use another <port>");
    bv = listen(listenfd, 10); //����������� � ��������� �����, backlog - ��� ����� ����������, ������� ���� ����� �������� � �������
    doOrdie(bv < 0, "ERROR7: unable to listen socket. Why? This is really strange");

    //��� SELECT
    int maxfd = listenfd;
    fd_set allset;
    FD_ZERO(&allset); //���, � ����� FD_CLR, FD_ISSET, FD_SET - �������
    FD_SET(listenfd, &allset);

    //������ ����������: ������ - ������ - ������
    std::list <Pipe> pipes;

    for(;;) {
        fd_set rset, wset;
        rset = wset = allset;
        FD_CLR(listenfd, &wset);
        int nready;
        while((nready = select(maxfd + 1, &rset, &wset, NULL, NULL) < 0)) {
            if (errno == ENOMEM) {
                pipes.pop_back();
            } else if (errno != EINTR) {
                printf("ERROR11: select. Why?\n");
                exit(0);
            }
        }

        if (FD_ISSET(listenfd, &rset)) {
            //��� �������������� ������ ������ - ������
            struct sockaddr_in cliaddr;
            unsigned int clilen = sizeof(cliaddr);
            int connfd = accept(listenfd, (struct sockaddr*) &cliaddr, &clilen);
            doOrNot(connfd < 0, "ERROR8: unable to connect to new client");

            //���������, ����� �� ��������� �ݣ ������ �������
            if (pipes.size() + 1 == FD_SETSIZE) {
                printf("ERROR10: i haven't memory for new client\n");
                close(connfd);
            } else {
                //��� ������ ������ - ������ ��� ����� �������
                int sockfd = socket(AF_INET, SOCK_STREAM, 0);
                doOrdie(sockfd == -1, "ERROR9: unable to create socket");
                memset(&servaddr, 0, sizeof(servaddr));
                servaddr.sin_family = AF_INET;
                servaddr.sin_port = htons(PORT);
                inet_pton(AF_INET, IP, &servaddr.sin_addr); //��������� ������������ IP � ������

                if (connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1) { //����� ���� �������� ��������, �� ����� �� ���� � ���� ������ ������ ������������ ����� ����� (�������, 130)
                    while (close(connfd) == -1) if (errno != EINTR) break;
                    while (close(sockfd) == -1) if (errno != EINTR) break;              
                } else {
                    pipes.push_back(Pipe::newPipe(connfd, sockfd, cliaddr));    
                    FD_SET(connfd, &allset);
                    FD_SET(sockfd, &allset);
                    if (maxfd < connfd) {
                        maxfd = connfd + 1;
                    }
                    if (--nready <= 0) {
                        continue;
                    }
                }
            }
        }

        std::list<Pipe>::iterator it; 
        it = pipes.begin(); 
        while (it != pipes.end())
        {
            if ((*it).isEmpty()) {
                it = pipes.erase(it);
            }
            int clientSocket = (*it).clientSocket;
            int serverSocket = (*it).serverSocket;
            bool closed = false;
            if (FD_ISSET(clientSocket, &rset)) {
                closed = (*it).readClient();
                --nready;
            }
            if (!closed && FD_ISSET(clientSocket, &wset)) {
                closed = (*it).writeClient();
                --nready;
            }
            if (!closed && FD_ISSET(serverSocket, &rset)) {
                closed = (*it).readServer();
                --nready;
            }
            if (!closed && FD_ISSET(serverSocket, &wset)) {
                closed = (*it).writeServer();
                --nready;
            }
            if (closed) {
                FD_CLR(clientSocket,&allset);
                FD_CLR(serverSocket,&allset);
                it = pipes.erase(it);
            } else {
                ++it; // � ��������� � ���������� ��������
            }
            if (nready == 0) break;
        }
    }
}