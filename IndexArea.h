#pragma once
#include "Area.h"

class IndexArea : public Area
{

public:
	using Area::Area;

	uint32_t GetRecord(std::ifstream& file, const uint32_t index) const;
};

