#include "IndexArea.h"

uint32_t IndexArea::GetRecord(std::ifstream& file, const uint32_t index) const
{
	uint32_t record;

	file.seekg(index, std::ifstream::beg);
	file.read(reinterpret_cast<char*>(&record), this->GetRecordSize());

	return record;
}
