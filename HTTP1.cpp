#include "HTTP.h"

void doOrDie(bool condition, const char* message) 
{
	if (condition) {
		printf("%s\n",message);
		exit(0);
	}
}

void doOrNot(bool condition, const char* message) 
{
    if (condition) {
        printf("%s\n",message);
    }
}

//сохраняем сюда данные, что пришли
//вроде как в запросе HTTP может быть \0 как часть данных
//поэтому новая структурка
ReqBuffer::ReqBuffer() 
{
	v = std::vector <char>();
}
void ReqBuffer::add(const char* buf, int n) 
{
	for (int i = 0; i < n; ++i) 
	{
		if (getSpaceSpace != 5 && getSpaceSpace !=6) {
			v.push_back(*(buf + i));
		}
		switch (getSpaceSpace) {
			case 0 : {
				if (*(buf + i) == 'G') ++getSpaceSpace;
				break;
			}
			case 1 : {
				if (*(buf + i) == 'E') ++getSpaceSpace;
				else getSpaceSpace = 0;
				break;
			}
			case 2 : {
				if (*(buf + i) == 'T') ++getSpaceSpace;
				else getSpaceSpace = 0;
				break;
			}	
			case 3 : {
				if (*(buf + i) == ' ') ++getSpaceSpace;
				else getSpaceSpace = 0;
				break;
			}	
			case 4 : {
				if (*(buf + i) == ' ' || *(buf + i) == '\r') {
				v.push_back('H');
				std::string s = "TTP/1.0\r\n";
				int sizeS = s.size();
				for (int i = 0; i < sizeS; ++i) {
					v.push_back(s[i]);
				}
				++getSpaceSpace;
				}
				break;
			}	
			case 5: {
				if (*(buf + i) == '\r') ++getSpaceSpace;
				break;
			}
			case 6: {
				if (*(buf + i) == '\n') ++getSpaceSpace;
				else --getSpaceSpace;
				break;
			}
			default: {}
		}
		int length = v.size();
		if (getSpaceSpace == 7 && !isEndToRead) isEndToRead = length > 3
				& (v[length - 4] == '\r')
				& (v[length - 3] == '\n')
				& (v[length - 2] == '\r')
				& (v[length - 1] == '\n');
		if (getSpaceSpace != 7 && (length > 3
				& (v[length - 4] == '\r')
				& (v[length - 3] == '\n')
				& (v[length - 2] == '\r')
				& (v[length - 1] == '\n'))) {
			getSpaceSpace = 7;
		}
		//if (isEndToRead) for (int j=0; j < v.size(); ++j) std :: cout << v[j];
	}
	/*int length = v.size();
	if (getSpaceSpace == 7 && !isEndToRead) isEndToRead = length > 3
				& (v[length - 4] == '\r')
				& (v[length - 3] == '\n')
				& (v[length - 2] == '\r')
				& (v[length - 1] == '\n');*/
}
//возвращает строку после первого вхождения header до end
std::string stringAfter(const std::string& header, ReqBuffer reqBuffer, char end) 
{
	int i = numberAfter(header, reqBuffer);
	std::string s = "";
	int size = reqBuffer.v.size();
	std::vector<char>& v = reqBuffer.v;
	while (i < size && v[i] != end) {
		s+=v[i];
		++i;
	}
	return s;
}

int numberAfter(const std::string& header, ReqBuffer reqBuffer)
{
	int headerSize = header.length();
	int size = reqBuffer.v.size() - headerSize;
	std::vector<char>& v = reqBuffer.v;
	int i, j;
	for (i = 0; i < size; ++i) 
	{
		j = 0;
		while (j < headerSize && v[i+j] == header[j]) {
			++j;
		}
		if (j == headerSize) {
			break;
		}
	}
	return i + j;
}

//header в виде "header"
std::string headerValue(std::string header, const ReqBuffer& reqBuffer) 
{
	header += ": ";
	return stringAfter(header, reqBuffer, '\r');
}

Request::Request(ReqBuffer& reqBuffer) : request(reqBuffer)
{
	std::string hostAndPort;
	size_t pos;
	try {
		hostAndPort = headerValue("Host", reqBuffer);
		if (hostAndPort == "") {
			return;
		}
		pos = hostAndPort.find(':');
		if ((pos = hostAndPort.find(':')) != std::string::npos) {
			host = hostAndPort.substr(0, pos);
			port = stoi(hostAndPort.substr(pos + 1, hostAndPort.length())); //может быть исключение
		} else {
			port = 80;
			host = hostAndPort;
		}

		int number = numberAfter("Connection: ", reqBuffer); //первое вхождение
			if (number + 5 < reqBuffer.v.size()) {
				reqBuffer.v[number] = 'c';
				std::string s = "lose";
				int sizeS = s.size();
				int i;
				for (i = 0; i + number < sizeS; ++i) {
					reqBuffer.v[number + i] = s[i];
				}
				i += number;
				while (i < reqBuffer.v.size() && reqBuffer.v[i] != '\r') {
					reqBuffer.v[number + i] = ' ';
					++i;
				}
		}

		strReq = "GET" + stringAfter("GET", reqBuffer, '\r');
	} catch (std::out_of_range a) {
		printf("ERROR REQUEST 1: incorrect request\n");
		host = ""; port = 0; strReq = "";
		return;
	} catch (std::invalid_argument a) {
		printf("ERROR REQUEST 3: incorrect port\n");
		host = ""; port = 0; strReq = "";
		return;
	}		
}

//здесь нормально, что ответ не вышел
Reply::Reply(ReqBuffer& reqBuffer) : request(reqBuffer)
{
	//printf("RY\n");
	try {
		mime = headerValue("Content-Type", reqBuffer);
		std::string afterHTTP = stringAfter("HTTP", reqBuffer, '\n');
		code = stoi(afterHTTP.substr(5, 3));

		if (code > 0) {
			int number = numberAfter("HTTP/1.", reqBuffer); //первое вхождение
			if (number < reqBuffer.v.size()) {
				reqBuffer.v[number] = '0';
			}
		}
	} catch (std::out_of_range a) {
		//printf("ERROR REPLY 2: incorrect reply\n");
		mime = ""; code = 0;
		//printf("RYEnd\n");
		return;
	} catch (std::invalid_argument a) {
		//printf("ERROR REPLY 3: incorrect code\n");
		mime = ""; code = 0;
		//printf("RYEnd\n");
		return;
	}		
}


/*
int main (int argc, char **argv) {
	ReqBuffer reqBuffer = ReqBuffer();*/
//	std::string s = "GET http://10.0.68.80:5053/ HTTP/1.1\nHost: 10.0.68.80:5053\nUser-Agent: curl/7.61.1\nAccept: */*\nProxy-Connection: Keep-Alive\r\n\r\n";
//	reqBuffer.add("GET http://10.0.68.80:5053/ HTTP/1.1\nHost: 10.0.68.80:5053\nUser-Agent: curl/7.61.1\nAccept: */*\nProxy-Connection: Keep-Alive\n\r\n\r", s.size());
/*	Request request = Request(reqBuffer);
	std::cout << request.host << " " << request.port << "\n";
	int size = request.request.v.size();
	for (int i=0; i<size; ++i) {
		 std::cout << request.request.v[i];
	}

	ReqBuffer reqBuffer1 = ReqBuffer();
	s = std::string("HTTP/1.1 200 OK\nDate: Sun, 25 Oct 2020 14:22:14 GMT\nServer: Apache/2.4.10 (Debian) PHP/5.6.40-0+deb8u7 OpenSSL/1.0.1t") +
					  std::string("\nLast-Modified: Fri, 20 Oct 2017 12:23:58 GMT\nETag: \"3a1-55bf98c17da3c\"\nAccept-Ranges: bytes\nContent-Length: 929") +
					  std::string ("\nVary: Accept-Encoding\nContent-Type: text/html; charset=koi8-r\n\r\n\r");
	reqBuffer1.add("HTTP/1.1 200 OK\nDate: Sun, 25 Oct 2020 14:22:14 GMT\nServer: Apache/2.4.10 (Debian) PHP/5.6.40-0+deb8u7 OpenSSL/1.0.1t\nLast-Modified: Fri, 20 Oct 2017 12:23:58 GMT\nETag: \"3a1-55bf98c17da3c\"\nAccept-Ranges: bytes\nContent-Length: 929\nVary: Accept-Encoding\nContent-Type: text/html; charset=koi8-r\n\r\n\r", s.size());
	Reply reply = Reply(reqBuffer1);
	std::cout << reply.mime << " " << reply.code << "\n";
	size = reply.request.v.size();
	for (int i=0; i<size; ++i) {
		 std::cout << reply.request.v[i];
	}
	return 0;
}

*/

struct ReqBuffer {
	std::vector<char> v;
	int cntW = 0;
	bool isEndToRead = false;

	int getSpaceSpace = 0; //когда станет 5, нужно подставлять HTTP1/0

	ReqBuffer();
	void add(const char* buf, int n);
};

//возвращает строку после первого вхождения header до end
std::string stringAfter(const std::string& header, ReqBuffer reqBuffer, char end);

int numberAfter(const std::string& header, ReqBuffer reqBuffer);

//header в виде "header"
std::string headerValue(std::string header, const ReqBuffer& reqBuffer);

struct Request {
	std::string host;
	int port;
	ReqBuffer& request;
	std::string strReq;

	Request(ReqBuffer& reqBuffer);
};

struct Reply {
	std::string mime;
	int code;
	ReqBuffer& request;

	//здесь нормально, что ответ не вышел
	Reply(ReqBuffer& reqBuffer);
};