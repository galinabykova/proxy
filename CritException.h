#include "biblio.h"

struct CritException {
	std::string errorStr;
	CritException(const char *);
	void printError();
};