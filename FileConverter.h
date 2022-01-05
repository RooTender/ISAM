#pragma once
#include <fstream>
#include <string>
#include <utility>

class FileConverter
{
	std::string directory;

	static bool isTextfileValid(const std::string& filename);
	static bool areStreamsOpened(std::ifstream& input, std::ofstream& output);

public:
	explicit FileConverter(std::string directory) : directory(std::move(directory))
	{
	}

	std::string textToBinary(std::string filename) const;
	std::string binaryToText(std::string filename) const;
};
