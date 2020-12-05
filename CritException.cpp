#ifndef _CRIT_EXCEPTION
#define _CRIT_EXCEPTION 1

#include "CritException.h"

CritException::CritException(const char* a) {
	errorStr = a;
}

void CritException::printError() {
	std::cout << errorStr << std::endl;
}

#endif