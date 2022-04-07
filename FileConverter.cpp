#include "FileConverter.h"

bool FileConverter::IsTextFileValid(const std::string& filename)
{
	auto input = std::ifstream(filename);
	if (!input.is_open())
	{
		input.close();

		throw std::runtime_error("Cannot open file! Is it used by another process?");
	}

	const auto records = std::count
	(
		std::istreambuf_iterator<char>(input),
		std::istreambuf_iterator<char>(),
		'\n'
	);
	input.close();

	if (records % 2 == 1)
	{
		throw std::runtime_error("Text file has contains odd number of records.");
	}

	return true;
}

bool FileConverter::AreStreamsOpened(std::ifstream& input, std::ofstream& output)
{
	if (!input.is_open() || !output.is_open())
	{
		output.close();
		input.close();
		throw std::runtime_error("Cannot open files! Are they used by another process?");
	}
	return true;
}

std::string FileConverter::TextToBinary(std::string filename) const
{
	filename = this->directory + "/" + filename;

	if (!IsTextFileValid(filename))
	{
		throw std::exception("Text file is not valid.");
	}

	auto inputName = filename + ".txt";
	auto outputName = filename + ".bin";

	auto input = std::ifstream(inputName, std::ofstream::binary);
	auto output = std::ofstream(outputName);

	if (!AreStreamsOpened(input, output))
	{
		throw std::exception("Cannot open IO streams.");
	}

	double number;
	while (input >> number)
	{
		output.write(reinterpret_cast<char*>(&number), sizeof(double));
	}

	output.close();
	input.close();

	return outputName;
}

std::string FileConverter::BinaryToText(std::string filename) const
{
	filename = this->directory + "/" + filename;
	auto inputName = filename + ".bin";
	auto outputName = filename + ".txt";

	auto input = std::ifstream(inputName, std::ofstream::binary);
	auto output = std::ofstream(outputName);

	if (!AreStreamsOpened(input, output))
	{
		throw std::exception("Cannot open IO streams.");
	}

	double number;
	while (input.good())
	{
		input.read(reinterpret_cast<char*>(&number), sizeof(double));
		if (input.eof())
		{
			break;
		}

		output << number << std::endl;
	}

	output.close();
	input.close();

	return outputName;
}
