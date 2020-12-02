#ifndef _HTTP
#define _HTTP 1

#include "CritException.h"
#include "biblio.h"

#define log(...) if (LOG) fprintf(stderr, __VA_ARGS__)

void doOrDie(bool condition, const char* message);

void doOrNot(bool condition, const char* message);

void addStr(std::vector <char>& v, const std::string s);

int updateState(int lastState, std::string s, char c, bool drop);

struct Request {
	std::vector<char> v;
	bool isEndToRead = false;
	int stateConnection = 0;
	int stateProxyConnection = 0;
	int stateGET = 0;
	int stateHost = 0;
	int stateEnd = 0;
	int skipBefore = 256;

	std::string host = "";
	std::string strReq;

	Request();
	void add(const char* buf, int n);
};

struct Reply {
	std::vector<char> v;
	bool isEndToRead = false;
	int stateConnection = 0;
	int stateProxyConnection = 0;
	int stateHTTP = 0;
	int stateMime = 0;
	int stateEnd = 0;
	int stateContentLength = 0;
	int skipBefore = 256;

	std::string mime = "";
	std::string codeStr = "";
	std::string contentLengthStr = "";
	int code;
	int cntEmptyStr = 0;
	long contentLength = 0;
	long cnt = 0;
	Reply();
	void add(const char* buf, int n);

};

#endif
