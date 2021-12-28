#pragma once
#include <string>
#include <fstream>

class DBMS
{
private:
	struct File {
		const std::string overflow	= "./overflow.bin";
		const std::string index		= "./index.bin";
		const std::string primary	= "./primary.bin";
	};

	void loadData();

public:
	DBMS();
};

