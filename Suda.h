#ifndef _SUDA
#define _SUDA 1

#include "biblio.h"
#include "HTTP.h"

extern bool LOG;

struct Suda {
	Request req;
	int indexNext = 0; //для записи на сервер request

	int serverSocket;

	//АККУРАТНО read использует char*, a &v[0] предполагает, что мы ничего не добавляем в вектор между &v[0] и использованием)

	//для получения ответа
	Reply reply; //нужен, поскольку нужно хранить весь запрос в одно время, но непонятен его размер
	static const int MAX_CNT_IN_ONE_TIMES = 5000;
	char* buf; //нужен, поскольку с ним работает read

	char ip[16]; //для логов
	int port = 80;  //тоже для логов

	bool error = false;
	std :: string mime = "";
	int code = 0; //кэшируем только 200
	int time = 0;

	bool tryToConnectNow = false;

	private :
	int cntOfReaders = 0; //количество читающих сейчас

	public:
	Suda(Request req);
	Suda(const Suda &copy);
	~Suda();

	void incCntOfReaders();
	void deqCntOfReaders();
	int getCntOfReaders();

	bool readServerToProxy();

	bool writeProxyToServer();

	bool isEnd(int clientIndex);
};

#endif