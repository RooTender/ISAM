#pragma once
// ReSharper disable once CppUnusedIncludeDirective
#include "FileUtils.h"
#include <string>


class Area
{
	std::string filename;
	size_t length = 0;
	size_t recordSize;

public:
	explicit Area(std::string filename, size_t areaRecordSize);

	void SetFilename(std::string name);
	std::string GetFilename() const;
	std::string GetBackupFilename() const;

	void UpdateLength();
	size_t GetLength() const;
	size_t GetRecordSize() const;
};

