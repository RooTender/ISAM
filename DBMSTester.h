#pragma once
#include "DBMS.h"
#include <time.h>

class DBMSTester : public DBMS
{
public:
	DBMSTester(uint32_t blockingFactor, double alpha);

	void createFakeIndexes(size_t from, size_t to);
};

