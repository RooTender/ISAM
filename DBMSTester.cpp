#include "DBMSTester.h"

DBMSTester::DBMSTester(uint32_t blockingFactor, double alpha) : DBMS(blockingFactor, alpha, 0.2)
{
}

void DBMSTester::createFakeIndexes(size_t from, size_t to)
{
	std::ofstream file = std::ofstream("./index.bin", std::ofstream::app | std::ofstream::binary);

	for (size_t i = from; i < to; ++i)
	{
		file.write(reinterpret_cast<char*>(&i), sizeof(uint32_t));
	}
	
	file.close();
}
