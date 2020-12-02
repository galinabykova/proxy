#ifndef _CACHE
#define _CACHE 1

#include "biblio.h"
#include "Suda.h"

extern bool LOG_CACHE;
extern bool LOG;

struct Cache 
{
	std::map <std::string, Suda*> cache_entries;
	int n_entries = 0; //нужен, чтобы генерировать ключи для некэшируемых

	Cache();
	Suda* add(Request req);
	void clear();
	~Cache();
};

#endif