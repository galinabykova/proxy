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
	Cache& cache;
	int clientSocket;

	//для получения GET запроса
	Request req; //нужен, поскольку нужно хранить весь запрос в одно время, но непонятен его размер
	static const int MAX_CNT_IN_ONE_TIMES = 5000;
	char* buf; //нужен, поскольку с ним работает read

	//для записи
	Suda* suda = NULL;
	int index; //индекс байта, который нужно записать следующим из SUDA

	char ip[16]; //для логов
	int port;  //тоже для логов

	bool error = false;

	Tuda(int clSocket, sockaddr_in cliaddr, Cache& c);
	Tuda(const Tuda &copy);

	Tuda& operator= (const Tuda &tuda) = delete; 

	bool readClientToProxy();

	bool writeProxyToClient();

	~Tuda();
};

#endif
