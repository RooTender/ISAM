#pragma once
#include <fstream>

class FileUtils
{
	static void MoveCursorTo(std::streampos position, std::ifstream& file);
	static void MoveCursorTo(std::streampos position, std::ofstream& file);
	static void MoveCursorToTheEnd(std::ifstream& file);
	static void MoveCursorToTheEnd(std::ofstream& file);

public:
	static std::string GetFilenameWithoutExtenstion(const std::string& filename);
	static std::string GetFilenameWithoutPathAndExtenstion(const std::string& filename);

	static size_t GetFileLength(std::ifstream& file);
	static size_t GetFileLength(std::ofstream& file);

	static void ChangeFileExtension(const std::string& filename, const std::string& newExtenstion, bool removeOldFileIfExists);
	static void CreateFile(const std::string& filename, bool recreateIfExists);
};

