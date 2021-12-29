#include "DBMS.h"

bool DBMS::isFileEmpty(std::ofstream& file)
{
	file.seekp(0, std::ios::end);
	return (file.tellp() == 0) ? true : false;
}

size_t DBMS::getFileLength(std::ifstream& file)
{
	const uint32_t currPos = file.tellg();

	file.seekg(0, file.end);
	size_t length = file.tellg();

	file.seekg(currPos, file.beg);
	return (length / sizeof(uint32_t));
}

uint32_t DBMS::getPageNumber(uint32_t key)
{
	std::ifstream file = std::ifstream(this->area.index, std::ifstream::binary);
	const size_t length = this->getFileLength(file);
	uint32_t pointer, left = 0, right = length, keyValue = 0;

	// Binary search
	while (left != right)
	{
		pointer = (left + right) / 2;
		file.seekg(pointer * sizeof(uint32_t), file.beg);

		file.read(reinterpret_cast<char*>(&keyValue), sizeof(uint32_t));

		if (keyValue > key) {
			right = pointer;
		}
		else {
			left = pointer;
		}
	}
	file.close();

	return keyValue;
}

DBMS::DBMS(uint32_t blockingFactor, double alpha)
{
	this->blockingFactor = blockingFactor;
	this->alpha = alpha;

	std::ofstream file = std::ofstream(this->area.primary, std::ofstream::app | std::ofstream::binary);
	if (this->isFileEmpty(file))
	{
		double number = 0;
		bool deleteFlag = true;

		// Generate dummy record
		file.write(reinterpret_cast<char*>(&number), sizeof(uint32_t));	// key
		file.write(reinterpret_cast<char*>(&number), sizeof(double));	// data: height
		file.write(reinterpret_cast<char*>(&number), sizeof(double));	// data: radius
		file.write(reinterpret_cast<char*>(&number), sizeof(Record*));	// pointer
		file.write(reinterpret_cast<char*>(&deleteFlag), sizeof(bool));	// delete flag 
	}
	file.close();


	file = std::ofstream(this->area.index, std::ofstream::app | std::ofstream::binary);
	if (this->isFileEmpty(file))
	{
		double number = 0;
		file.write(reinterpret_cast<char*>(&number), sizeof(uint32_t));	// key
	}
	file.close();


	file = std::ofstream(this->area.overflow, std::ofstream::app | std::ofstream::binary);
	file.close();
}

void DBMS::update()
{

}

void DBMS::insert(uint32_t key, Record record)
{

}

void DBMS::remove(uint32_t key)
{
}

void DBMS::read(uint32_t key)
{
}