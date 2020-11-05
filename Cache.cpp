#include "Cache.h"

//тут Cache - это все Suda, даже те, что кэшировать не надо
//у некэшируемых код не 200
//у старых записей мы меняем code, чтоб они тож больше не кэшировались

Cache::Cache() 
{
	m = std::map <std::string, Suda*>();
}

Suda* Cache::add(Request req) 
{
	std::string key = req.strReq;
	if (m.find(key) != m.end()) {
		Suda *suda = m[key];
    if (suda -> error) {
      if (LOG_CACHE) printf("error, I can't use cache\n");
      m[std::to_string(ind)] = suda;
      ++ind;
    } else {
		  if (suda -> code == 200) { 
        if (LOG_CACHE) printf("Use from cache\n");
        return suda;
		  } else {
        if (LOG_CACHE) printf("code %d != 200, I can't use cache\n", suda -> code);
        m[std::to_string(ind)] = suda;
        ++ind;
		  }
    }
	}
	Suda* ptr = new Suda(req);
	m[key] = ptr;  //на время key
  ++ind;
	if (LOG_CACHE) printf("new suda %s\n", key.c_str());
	return ptr;
}

void Cache::clear() 
{
  	/*std::map <std::string, Suda*> ::iterator it; 
    it = m.begin(); 
    while (it != m.end())
    {
    	//можно потом сделать так, что каждая Suda в своём потоке сама за себя отвечает
       	//тут всё равно cntOfReaders надо защитить
       	if ((*it).second -> time >= 900) {
       		if ((*it).second -> getCntOfReaders() == 0) {
       			delete[] (*it).second;
            (*it).second = NULL;
       			it = m.erase(it);
       			if (LOG_CACHE) printf("deleted\n");
       		} else {
       			if (LOG_CACHE && (*it).second -> code != 10) printf("time to use the recording in cache is over\n");
       			(*it).second -> code = 10;
       		}
       	} else {
       		//++((*it).second -> time);
       	} 		
   		++it;
   	}*/
}

Cache::~Cache() 
{
	std::map <std::string, Suda*> ::iterator it; 
    it = m.begin(); 
    while (it != m.end())
    {
      	delete[] (*it).second;  
       	++it; 
    }
}
