#include "Area.h"

void Area::SetFilename(std::string name)
{
	this->filename = std::move(name);
}

std::string Area::GetFilename() const
{
	return this->filename;
}

void Area::UpdateLength()
{
	auto file = std::ifstream(this->filename, std::ifstream::binary);
	this->length = FileUtils::GetFileLength(file);

	file.close();
}

size_t Area::GetLength() const
{
	return this->length;
}
