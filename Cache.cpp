#include <map>
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
#include <vector>
#include "Suda.cpp"

extern bool LOG; //делать логирование

struct Cache {
	std :: map <std :: string, Suda*> m;
	int ind = 0; //нужен, чтобы генерировать ключи для некэшируемых

	Cache() {
		m = std :: map <std :: string, Suda*>();
	}

	Suda* add(ReqBuffer reqBuffer) {
		Request request = Request(reqBuffer);
		std :: string key = request.strReq;
		if (m.find(key) != m.end()) {
			Suda *suda = m[key];
			if (suda -> mime != "" && suda -> code == 200) { 
				if (1 || LOG) printf("Use from cache\n");
				return suda;
			} else {
				m[std :: to_string(ind)] = suda;
				++ind;
			}
		}
		Suda* ptr = new Suda(reqBuffer);
		m[key] = ptr;
		if (1 || LOG) printf("new suda\n");
		return ptr;
	}

	void clear() {
		std::map<std :: string, Suda*>::iterator it; 
        it = m.begin(); 
        while (it != m.end())
        {
        	//можно потом сделать так, что каждая Suda в своём потоке сама за себя отвечает
        	//тут всё равно cntOfReaders надо защитить
        	if ((*it).second -> getCntOfReaders() == 0) {
        		if ((*it).second -> time == 50) {
        			delete[] (*it).second;
        			it = m.erase(it);
        			printf("deleted\n");
        		} else {
        			//и time тоже?
        			++((*it).second -> time);
        		}
        	} else {
        		(*it).second -> time = 0;
        	}
        	++it;
        }
	}

	~Cache() {
		std::map<std :: string, Suda*>::iterator it; 
        it = m.begin(); 
        while (it != m.end())
        {
        	delete[] (*it).second;  
        	++it; 
        }

	}
};