#pragma once
#include <fstream>
#include <string>
#include <utility>

class FileConverter
{
	std::string directory;

	static bool IsTextFileValid(const std::string& filename);
	static bool AreStreamsOpened(std::ifstream& input, std::ofstream& output);

public:
	explicit FileConverter(std::string directory) : directory(std::move(directory))
	{
	}

	std::string TextToBinary(std::string filename) const;
	std::string BinaryToText(std::string filename) const;
};
