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

extern bool LOG; //������ �����������
extern bool LINE_BY_LINE; //��� 26 ����
extern int MAX_STR;

//��� ��������� ����� ������������ ������ � �����-�� ����������
//(������ ���, ����� � ţ ������, ��� ������� � ������,
// ��� ��� ţ ����� ��������������)
struct Pipe {
	int clientSocket;
	int serverSocket; //����� ����, ���� �� �ӣ �����������

	//������ ��������� � �����, ������ ���
	//����� ���������� ��������, �����
	//�� ������� �� ������ ������ ����� ����
	//� � ������ ������� �� ����� ��������
	char *bufClientToServer;
	char *iptrClientToServer; //���� � ������ �����
	char *optrClientToServer; //������ � ������ ������
	int stdineofClientToServer;

	char *bufServerToClient;
	char *iptrServerToClient; //���� � ������ �����
	char *optrServerToClient; //������ � ������ ������	
	int stdineofServerToClient;

	char ip[16]; //��� �����
	int port;  //���� ��� �����
	int cntStr; //��� 26 ����

	static const int BUF_SIZE = 1024;

	static Pipe newPipe(int ClientSocket, int ServerSocket, sockaddr_in cliaddr){
		return Pipe(ClientSocket, ServerSocket, cliaddr);
	}

	static Pipe newPipe(int ClientSocket, int ServerSocket) {
		return Pipe(ClientSocket, ServerSocket);
	}

	Pipe& operator= (const Pipe &pipe) = delete; 

	//��� ����������� ������ �� ������ ��������, �� ��� ����� � �� �����
	//� ��, ������ ����������� �� ���� ������ ������������
	Pipe(const Pipe &copy) {
		clientSocket = copy.clientSocket;
		serverSocket = copy.serverSocket;

		iptrClientToServer = optrClientToServer = bufClientToServer = new char[BUF_SIZE];
		iptrServerToClient = optrServerToClient = bufServerToClient = new char[BUF_SIZE];
		strcpy(ip, copy.ip); //��������� ��-������, ������ ��� ��� ����� ������ ������������� char[]
		port = copy.port;

		stdineofClientToServer = false;
		stdineofServerToClient = false;
		cntStr = 0; //���� 26

        //����� �� ���� ���� pipe, ���������� � ������ ��������
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

	//�������, ��� pipe �� ����
	bool readClient() {
		int n = readP(clientSocket, serverSocket, bufClientToServer, &iptrClientToServer, &optrClientToServer, &stdineofClientToServer);
		if (LOG && n > 0) printf("client %s : %d - client -> proxy - %d bytes\n", ip, port, n);
		if (stdineofServerToClient == 2 || stdineofClientToServer == 2) {
			markEmpty();
			return true;
		}
		return false;
	}

	//�������, ��� pipe �� ����
	bool readServer() {
		int n = readP(serverSocket, clientSocket, bufServerToClient, &iptrServerToClient, &optrServerToClient, &stdineofServerToClient);
		if (LOG && n > 0) printf("client %s : %d - server -> proxy - %d bytes\n", ip, port, n);
		if (stdineofServerToClient == 2 || stdineofClientToServer == 2) {
			markEmpty();
			return true;
		}
		return false;
	}

	//�������, ��� pipe �� ����
	bool writeServer() { //?
		int n = writeP(clientSocket, serverSocket, bufClientToServer, &iptrClientToServer, &optrClientToServer, &stdineofClientToServer);
		if (LOG && n > 0) printf("client %s : %d - client <- proxy - %d bytes\n", ip, port, n);
		if (stdineofServerToClient == 2 || stdineofClientToServer == 2) {
			markEmpty();
			return true;
		}
		return false;
	}

	//�������, ��� pipe �� ����
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
		cntStr = 0; //���� 26
	}

	Pipe(int pClientSocket, int pServerSocket) : clientSocket(pClientSocket), serverSocket(pServerSocket), stdineofClientToServer(0), stdineofServerToClient(0) {
		iptrClientToServer = optrClientToServer = bufClientToServer = NULL;
		iptrServerToClient = optrServerToClient = bufServerToClient = NULL;
		strcpy(ip, "");
		port = 0;
		cntStr = 0; //���� 26
	}

	//�������, ��� Pipe �� ����
	int readP(int socketToRead, int socketToWrite, char *buf, char **iptr, char **optr, int *stdineof) {
		int n = 0;
		int cnt = buf + BUF_SIZE - (*iptr);
		while ((n = read(socketToRead, *iptr, cnt)) < 0) {
			if (errno == EINTR) {											//���� ��� �������� �� �������, ����� ������ � ������� � ���� ��� ��� ��� ���
				continue;											
			} else if (errno == EWOULDBLOCK) {
				break;
			} else {
				printf("ERROR PIPE 1: read\n");
				return 0; //���-�� ������?
			} 
		}
		if (n == 0 && cnt > 0) { //0 ������ ����� ���� ������� � 2 �������: EOF � ��� ����� � ������
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

	//�������, ��� Pipe �� ����
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
				return 0; //���-�� ������?
			} 
		}
		if (n >= 0) {
			if (LINE_BY_LINE) for (i = 0; i < n; ++i) if ((*optr)[i] == '\n') ++cntStr; //26 ����
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
