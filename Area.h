#pragma once
// ReSharper disable once CppUnusedIncludeDirective
#include "FileUtils.h"
#include <string>

class Area
{
	std::string filename;
	size_t length = 0;

public:
	void SetFilename(std::string name);
	std::string GetFilename() const;

	void UpdateLength();
	size_t GetLength() const;
};

