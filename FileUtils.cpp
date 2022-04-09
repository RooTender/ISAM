#include "FileUtils.h"

void FileUtils::MoveCursorTo(const std::streampos position, std::ifstream& file)
{
	file.seekg(position, std::ifstream::beg);
}

void FileUtils::MoveCursorTo(const std::streampos position, std::ofstream& file)
{
	file.seekp(position, std::ofstream::beg);
}

void FileUtils::MoveCursorToTheEnd(std::ifstream& file)
{
	file.seekg(0, std::ifstream::end);
}

void FileUtils::MoveCursorToTheEnd(std::ofstream& file)
{
	file.seekp(0, std::ofstream::end);
}

std::string FileUtils::GetFilenameWithoutExtenstion(const std::string& filename)
{
	return filename.substr(0, filename.find_last_of('.'));
}

std::string FileUtils::GetFilenameWithoutPathAndExtenstion(const std::string& filename)
{
	const auto cleanFilename = GetFilenameWithoutExtenstion(filename);
	return cleanFilename.substr(cleanFilename.find_last_of('/') + 1, cleanFilename.length());
}

size_t FileUtils::GetFileLength(std::ifstream& file)
{
	const std::streampos previousPosition = file.tellg();

	MoveCursorToTheEnd(file);
	const std::streampos length = file.tellg();

	MoveCursorTo(previousPosition, file);
	return length;
}

size_t FileUtils::GetFileLength(std::ofstream& file)
{
	const std::streampos previousPosition = file.tellp();

	MoveCursorToTheEnd(file);
	const std::streampos length = file.tellp();

	MoveCursorTo(previousPosition, file);
	return length;
}

void FileUtils::ChangeFileExtension(const std::string& filename, const std::string& newExtenstion, const bool removeOldFileIfExists)
{
	const std::string newFile = GetFilenameWithoutExtenstion(filename) + newExtenstion;

	if (removeOldFileIfExists)
	{
		if (std::remove(newFile.c_str()) != 0)
		{
			// Fail means that the file didn't exist
		}
	}

	if (std::rename(filename.c_str(), newFile.c_str()) != 0)
	{
		throw std::runtime_error("Failed to rename file!");
	}
}

void FileUtils::CreateFile(const std::string& filename, bool recreateIfExists)
{
	std::ifstream file;

	if (recreateIfExists)
	{
		file = std::ifstream(filename, std::ifstream::binary);
	}
	else
	{
		file = std::ifstream(filename, std::ifstream::binary | std::ifstream::app);
	}
	
	file.close();
}
