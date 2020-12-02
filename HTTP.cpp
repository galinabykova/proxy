#include "biblio.h"
#include "HTTP.h"

void doOrDie(bool condition, const char* message) 
{
	if (condition) {
		throw CritException(message);
	}
}

void doOrNot(bool condition, const char* message) 
{
    if (condition) {
        printf("%s\n",message); //незначительные ошибки выводит в stdin
    }
}

void addStr(std::vector <char>& v, const std::string s) 
{
	int stringSize = s.size();
	for (int i = 0; i < stringSize; ++i) {
		v.push_back(s[i]);
	}
}

int updateState(int lastState, std::string s, char c, bool drop) 
{
	if (lastState > s.size()) return lastState;
	if (c == s[lastState]) return ++lastState;
	if (drop) return 0;
	return lastState;
}

Request::Request() 
{
	v = std::vector <char>();
}


std::string withoutHost(std::string URL) {
	try {
		if (URL.find("http:", 0) != 0) return URL;
		std :: string withoutScheme = URL.substr(5, URL.length());
		std :: string hostAndAfter;
		size_t pos;
		if ((pos = withoutScheme.find('@')) != std :: string :: npos) {
			hostAndAfter = withoutScheme.substr(pos+1, withoutScheme.length());
		} else {
			hostAndAfter = withoutScheme.substr(2, withoutScheme.length());
		}
		return hostAndAfter.substr(hostAndAfter.find('/'), hostAndAfter.length());
	} catch (std :: out_of_range a) {
		return URL;
	} 
} 

void Request::add(const char* buf, int n) 
{
	for (int i=0; i<n; ++i) {
		if (skipBefore != 256) {
			if (skipBefore == buf[i]) {
				skipBefore = 256;
				v.push_back(buf[i]);
			} 
			stateEnd = updateState(stateEnd, "\r\n\r\n", buf[i], true);
			continue;
		}
		stateConnection = updateState(stateConnection, "\nConnection:", buf[i], true);
		if (stateConnection == 12) {
			v.push_back(buf[i]);
			addStr(v, " close");
			++stateConnection;
			skipBefore = '\r';
			continue;
		}
		stateGET = updateState(stateGET, "GET ", buf[i], true);
		if (stateGET == 4) {
			strReq = "";
			v.push_back(buf[i]);
			++stateGET;
			continue;
		}
		if (stateGET == 5) {
			//strReq += buf[i];
			if (buf[i] == ' ' || buf[i] == '\r') {
				addStr(v, withoutHost(strReq));
				addStr(v, " HTTP/1.0"); 
				++stateGET;
				skipBefore = '\r';
			}
			strReq += buf[i];
			if (buf[i] == '\r') {
				skipBefore = 256;
				v.push_back('\r'); //????
			}
		//	if (buf[i] != ' ') v.push_back(buf[i]);
			continue;
		}
		v.push_back(buf[i]);
		stateHost = updateState(stateHost, "Host: ", buf[i], true);
		if (stateHost == 6) {
			++stateHost;
			continue;
		}
		if (stateHost == 7) {
			if (buf[i] == ' ' || buf[i] == '\r') {
				++stateHost;
			} else {
				host += buf[i];
			}
		}
		stateEnd = updateState(stateEnd, "\r\n\r\n", buf[i], true);
		if (stateEnd == 4) {
			if (stateConnection < 12) {
				for (int i=0; i<2; ++i) v.pop_back();
				addStr(v, "Connection: close\r\n\r\n");
			}
			isEndToRead = true;
			strReq += host;
		}
	}
}

Reply::Reply() 
{
	v = std::vector <char>();
}

void Reply::add(const char* buf, int n) 
{
	for (int i=0; i<n; ++i) {
		if (stateEnd == 4) { //читаем данные, не заголовки
			v.push_back(buf[i]);
			++cnt;
			continue;
		}
		if (skipBefore != 256) {
			if (skipBefore == buf[i]) {
				skipBefore = 256;
				v.push_back(buf[i]);
			}
			stateEnd = updateState(stateEnd, "\r\n\r\n", buf[i], true); 
			continue;
		}
		stateConnection = updateState(stateConnection, "\nConnection:", buf[i], true); //????????7
		if (stateConnection == 12) {
			v.push_back(buf[i]);
			addStr(v, " close");
			++stateConnection;
			skipBefore = '\r';
			continue;
		}
		stateHTTP = updateState(stateHTTP, "HTTP/", buf[i], true);
		if (stateHTTP == 5) {
			v.push_back(buf[i]);
			addStr(v, "1.0");
			skipBefore = ' ';
			++stateHTTP;
			continue;
		}
		v.push_back(buf[i]);
		if (stateHTTP == 6) {
			codeStr += buf[i];
			if (codeStr.size() == 3) {
				try{
					code = stoi(codeStr);
				} catch (std::invalid_argument a) {
					code = 0;
				}		
				++stateHTTP;
			}
			continue;
		}
		stateMime = updateState(stateMime, "Content-Type: ", buf[i], true);
		if (stateMime == 14) {
			++stateMime;
			continue;
		}
		if (stateMime == 15) {
			if (buf[i] == ' ' || buf[i] == '\r') {
				++stateMime;
			} else {
				mime += buf[i];
			}
		}
		stateContentLength = updateState(stateContentLength, "Content-Length: ", buf[i], true);
		if (stateContentLength == 16) {
			++stateContentLength;
			continue;
		}
		if (stateContentLength == 17) {
			if (buf[i] == ' ' || buf[i] == '\r') {
				++stateContentLength;
			} else {
				contentLengthStr += buf[i];
			}
		}
		if (stateContentLength == 18) {
			try{
				contentLength = stol(contentLengthStr);
				//std::cout<<contentLength<<std::endl;
			} catch (std::invalid_argument a) {
				contentLength = 0;
			}	
			++stateContentLength;
		}
		stateEnd = updateState(stateEnd, "\r\n\r\n", buf[i], true);
		//stateEndEnd = updateState(stateEnd, "\r\n\r\n\r\n", buf[i], true);
		if (stateEnd == 4) {
			if (stateConnection < 12) {
				for (int i=0; i<2; ++i) v.pop_back();
				addStr(v, "Connection: close\r\n\r\n");
			}
		}
	}
	if (cnt == contentLength/* || stateEndEnd == 6*/) {
		isEndToRead = true;
	}
}
	
/*
int main() {
	Request request = Request();
	std::string s = 
	"GET /wiki/страница HTTP/1.1\r\nHost: ru.wikipedia.org\r\nUser-Agent: Mozilla/5.0 (X11; U; Linux i686; ru; rv:1.9b5) Gecko/2008050509 Firefox/3.0b5\r\nConnection: fdjj\r\nAccept: text/html\r\n\r\n";
	request.add(s.c_str(), s.size());
	for (int i=0; i<request.v.size(); ++i) {
		std::cout << request.v[i];
	}
	std::cout << std::endl << request.host << std::endl << request.isEndToRead << std::endl;

	Reply reply = Reply();
	s =
	"HTTP/1.1 206 Partial Content\r\nAccept-Ranges: bytes\r\nContent-Range: bytes 64397516-80496894/160993792\r\nConnection: fdjj\r\nContent-Type: text/plain; charset=windows-1251\r\nContent-Length: 16099379\r\n";
	reply.add(s.c_str(), s.size());
	for (int i=0; i<reply.v.size(); ++i) {
		std::cout << reply.v[i];
	}
	std::cout << std::endl << reply.code << std::endl << reply.isEndToRead << std::endl << reply.mime << std::endl;
}
*/

/*
if (stateEnd == 4) {
			/*if (stateConnection < 12) {
				for (int i=0; i<2; ++i) v.pop_back();
				addStr(v, "Connection: close\r\n\r\n");
			}*//*
			isEndToRead = true;
			strReq += host;
		}
*/