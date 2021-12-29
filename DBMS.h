#pragma once
#include "Record.h"
#include <string>
#include <fstream>

class DBMS
{
private:
	struct Area {
		const std::string overflow	= "./overflow.bin";
		const std::string index		= "./index.bin";
		const std::string primary	= "./primary.bin";
	} area;

	uint32_t blockingFactor;
	double alpha;

	bool isFileEmpty(std::ofstream& file);
	size_t getFileLength(std::ifstream& file);

	uint32_t getPageNumber(uint32_t key);

public:
	DBMS(uint32_t blockingFactor, double alpha);

	void update();
	void insert(uint32_t key, Record record);
	void remove(uint32_t key);
	void read(uint32_t key);
};

