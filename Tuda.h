#ifndef _TUDA
#define _TUDA 1

#include "biblio.h"
#include "Cache.h"
#include "Suda.h"

extern bool LOG_CACHE;
extern bool LOG;

extern Cache cache;
extern fd_set allset;
extern int maxfd;

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

	Tuda(int clSocket, sockaddr_in cliaddr);
	Tuda(const Tuda &copy);

	Tuda& operator= (const Tuda &tuda) = delete; 

	bool readClientToProxy();

	bool writeProxyToClient();

	~Tuda();
};

#endif
