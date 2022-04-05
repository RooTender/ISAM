#pragma once
#include <fstream>

class FileUtils
{
private:
	static void MoveCursorTo(std::streampos position, std::ifstream& file);
	static void MoveCursorTo(std::streampos position, std::ofstream& file);
	static void MoveCursorToTheEnd(std::ifstream& file);
	static void MoveCursorToTheEnd(std::ofstream& file);

public:
	static size_t GetFileLength(std::ifstream& file);
	static size_t GetFileLength(std::ofstream& file);
};

