#pragma once
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <string>

class FileConverter
{
private:
	std::string directory;

	bool isTextfileValid(std::string filename);
	bool areStreamsOpened(std::ifstream& input, std::ofstream& output);

public:
	FileConverter(std::string directory) {
		this->directory = directory;
	}

	std::string textToBinary(std::string filename);
	std::string binaryToText(std::string filename);
};