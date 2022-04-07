#pragma once
#include "Area.h"

class IndexArea : public Area
{

public:
	using Area::Area;
	IndexArea() = delete;

	unsigned GetRecord(std::ifstream& file, const unsigned index) const;
};

