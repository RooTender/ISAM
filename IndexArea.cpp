#include "IndexArea.h"

unsigned IndexArea::GetRecord(std::ifstream& file, const unsigned index) const
{
	unsigned record = 0;

	file.seekg(index, std::ifstream::beg);
	file.read(reinterpret_cast<char*>(&record), static_cast<std::streamsize>(this->GetRecordSize()));

	return record;
}
