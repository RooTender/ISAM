#include "Area.h"

Area::Area(std::string filename, const size_t areaRecordSize) : filename(std::move(filename)), recordSize(areaRecordSize)
{
	FileUtils::CreateFile(filename, false);
	this->UpdateLength();
}

void Area::SetFilename(std::string name)
{
	this->filename = std::move(name);
}

std::string Area::GetFilename() const
{
	return this->filename;
}

std::string Area::GetBackupFilename() const
{
	return FileUtils::GetFilenameWithoutExtenstion(this->filename) + ".old";
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

size_t Area::GetRecordSize() const
{
	return this->recordSize;
}
