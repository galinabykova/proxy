#include "Cache.h"

//тут Cache - это все Suda, даже те, что кэшировать не надо
//у некэшируемых код не 200
//у старых записей мы меняем code, чтоб они тож больше не кэшировались

Cache::Cache() 
{
 // n_entries. cache_entries, n_entries
	cache_entries = std::map <std::string, Suda*>();
}

Suda* Cache::add(Request req) 
{
	std::string key = req.strReq;
	if (cache_entries.find(key) != cache_entries.end()) {
		Suda *suda = cache_entries[key];
    if (suda -> error) {
      log("error, I can't use cache\n");
      cache_entries[std::to_string(n_entries)] = suda;
      ++n_entries;
    } else {
		  if (suda -> code == 200) { 
        log("Use from cache\n");
        return suda;
		  } else {
        log("code %d != 200, I can't use cache\n", suda -> code);
        cache_entries[std::to_string(n_entries)] = suda;
        ++n_entries;
		  }
    }
	}
	Suda* ptr = new Suda(req);
	cache_entries[key] = ptr;  //на время key
  ++n_entries;
	log("new suda %s\n", key.c_str());
	return ptr;
}

void Cache::clear() 
{
  	std::map <std::string, Suda*> ::iterator it; 
    it = cache_entries.begin(); 
    while (it != cache_entries.end())
    {
    	//можно потом сделать так, что каждая Suda в своём потоке сама за себя отвечает
       	//тут всё равно cntOfReaders надо защитить
       	if ((*it).second -> time >= 900) {
       		if ((*it).second -> getCntOfReaders() == 0) {
       			it = cache_entries.erase(it);
       			log("deleted\n");
       		} else {
       			if ((*it).second -> code != 10) log("time to use the recording in cache is over\n");
       			(*it).second -> code = 10;
       		}
       	} else {
       		++((*it).second -> time);
       	} 		
   		++it;
   	}
}

Cache::~Cache() 
{
	std::map <std::string, Suda*> ::iterator it; 
    it = cache_entries.begin(); 
    while (it != cache_entries.end())
    {
      	delete[] (*it).second;  
       	++it; 
    }
}
