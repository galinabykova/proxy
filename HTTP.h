#ifndef _HTTP
#define _HTTP 1

#include "biblio.h"

void doOrDie(bool condition, const char* message);

void doOrNot(bool condition, const char* message);

struct ReqBuffer {
	std::vector<char> v;
	int cntW = 0;
	bool isEndToRead = false;

	ReqBuffer();
	void add(const char* buf, int n);
};

//возвращает строку после первого вхождения header до end
std::string stringAfter(const std::string& header, ReqBuffer reqBuffer, char end);

//header в виде "header"
std::string headerValue(std::string header, const ReqBuffer& reqBuffer);

struct Request {
	std::string host;
	int port;
	ReqBuffer request;
	std::string strReq;

	Request(const ReqBuffer& reqBuffer);
};

struct Reply {
	std::string mime;
	int code;
	ReqBuffer request;

	//здесь нормально, что ответ не вышел
	Reply(const ReqBuffer& reqBuffer);
};

#endif
