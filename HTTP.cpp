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
		v.push_back(*(buf + i));
	}
	int length = v.size();
	isEndToRead = length > 3;
	isEndToRead &= (v[length - 4] == '\r');
	isEndToRead &= (v[length - 3] == '\n');
	isEndToRead &= (v[length - 2] == '\r');
	isEndToRead &= (v[length - 1] == '\n');
}

//возвращает строку после первого вхождения header до end
std::string stringAfter(const std::string& header, ReqBuffer reqBuffer, char end) 
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
	i += j;
	std::string s = "";
	size = reqBuffer.v.size();
	while (i < size && v[i] != end) {
		s+=v[i];
		++i;
	}
	return s;
}

//header в виде "header"
std::string headerValue(std::string header, const ReqBuffer& reqBuffer) 
{
	header += ": ";
	return stringAfter(header, reqBuffer, '\r');
}

Request::Request(const ReqBuffer& reqBuffer) {
	request = reqBuffer;
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
Reply::Reply(const ReqBuffer& reqBuffer) {
	request = reqBuffer;
	try {
		mime = headerValue("Content-Type", reqBuffer);
		std::string afterHTTP = stringAfter("HTTP", reqBuffer, '\n');
		code = stoi(afterHTTP.substr(5, 3));
	} catch (std::out_of_range a) {
		//printf("ERROR REPLY 2: incorrect reply\n");
		mime = ""; code = 0;
		return;
	} catch (std::invalid_argument a) {
		//printf("ERROR REPLY 3: incorrect code\n");
		mime = ""; code = 0;
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