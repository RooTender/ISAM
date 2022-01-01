#include "DBMS.h"

bool DBMS::isFileEmpty(std::ofstream& file)
{
	file.seekp(0, std::ios::end);
	return (file.tellp() == 0) ? true : false;
}

size_t DBMS::getFileLength(std::ifstream& file)
{
	const std::streampos currPos = file.tellg();

	file.seekg(0, file.end);
	const std::streampos length = file.tellg();

	file.seekg(currPos, file.beg);
	return static_cast<size_t>(length);
}

size_t DBMS::getFileLength(std::ofstream& file)
{
	const std::streampos currPos = file.tellp();

	file.seekp(0, file.end);
	const std::streampos length = file.tellp();

	file.seekp(currPos, file.beg);
	return static_cast<size_t>(length);
}

void DBMS::moveCursorToTheNextAreaRecord(std::ofstream& file)
{
	const uint32_t cursor = file.tellp();
	file.seekp(cursor + mainRecordSize);
}


uint32_t DBMS::getIndexRecord(std::ifstream& file, uint32_t index)
{
	uint32_t record;

	file.seekg(index, file.beg);
	file.read(reinterpret_cast<char*>(&record), indexRecordSize);

	return record;
}

uint32_t DBMS::binarySearchPage(uint32_t key)
{
	const size_t
		indexPageSize = ((blockingFactor * mainRecordSize) / indexRecordSize) * indexRecordSize,
		pages = (this->area.length.index / indexPageSize) + 1;
	uint32_t left = 0, right = pages, pointer = 0;

	// Manage space to prevent overflow
	delete[] this->diskPage;
	uint32_t* indexPage = new uint32_t[indexPageSize];
	size_t indexPageLength = 0;

	// External binary search
	bool found = false;
	while (!found)
	{
		uint32_t first, last;

		this->diskOperations++;
		std::ifstream file = std::ifstream(this->area.index, std::ifstream::binary);

		pointer = (left + right) / 2;

		// First page record
		first = this->getIndexRecord(file, pointer * indexPageSize);

		// Last page record
		bool fullPageWasRead = true;
		if ((pointer + 1) * indexPageSize - indexRecordSize > this->area.length.index)
		{
			last = this->getIndexRecord(file, this->area.length.index - indexRecordSize);
			fullPageWasRead = false;
		}
		else
		{
			last = this->getIndexRecord(file, (pointer + 1) * indexPageSize - indexRecordSize);
		}

		if ((first <= key && last >= key) || !fullPageWasRead)
		{
			file.seekg(pointer * indexPageSize, file.beg);
			do
			{
				file.read(reinterpret_cast<char*>(&indexPage[indexPageLength]), indexRecordSize);
			} while (indexPageLength < indexPageSize && indexPage[indexPageLength++] != last);

			found = true;
		}
		else
		{
			if (key < first)
			{
				right = pointer;
			}
			else
			{
				left = pointer;
			}
		}

		file.close();
	}

	left = 0;
	right = indexPageLength;

	// Internal binary search
	while (left != right)
	{
		pointer = (left + right) / 2;

		if (key < indexPage[pointer])
		{
			right = pointer;
		}
		else
		{
			left = pointer;

			right = (right - left != 1) ? right : right - 1; // fix stuck on 0.5 issue
		}
	}

	// Restore previous space management
	delete[] indexPage;
	this->diskPage = new AreaRecord[blockingFactor];

	return pointer + 1;	// returns position, not index
}


AreaRecord DBMS::getAreaRecord(std::ifstream& file)
{
	const std::streampos currPos = file.tellg();
	uint32_t key, pointer;
	double radius, height;
	bool deleteFlag;

	file.read(reinterpret_cast<char*>(&key), sizeof(uint32_t));		// key
	file.read(reinterpret_cast<char*>(&radius), sizeof(double));	// data: height
	file.read(reinterpret_cast<char*>(&height), sizeof(double));	// data: radius
	file.read(reinterpret_cast<char*>(&pointer), sizeof(uint32_t));	// pointer
	file.read(reinterpret_cast<char*>(&deleteFlag), sizeof(bool));	// delete flag

	return AreaRecord(key, Record(radius, height), pointer, deleteFlag);
}

AreaRecord DBMS::getAreaRecord(std::ifstream& file, uint32_t index)
{
	const std::streampos currPos = file.tellg();

	file.seekg((index - 1) * mainRecordSize, file.beg);
	AreaRecord areaRecord = this->getAreaRecord(file);

	file.seekg(currPos);

	return areaRecord;
}

void DBMS::appendAreaRecord(std::ofstream& file, AreaRecord record)
{
	uint32_t key = record.key, pointer = record.pointer;
	double
		radius = record.data.getInput().first,
		height = record.data.getInput().second;
	bool deleteFlag = record.deleteFlag;

	file.write(reinterpret_cast<char*>(&key), sizeof(uint32_t));	// key
	file.write(reinterpret_cast<char*>(&radius), sizeof(double));	// data: height
	file.write(reinterpret_cast<char*>(&height), sizeof(double));	// data: radius
	file.write(reinterpret_cast<char*>(&pointer), sizeof(uint32_t));// pointer
	file.write(reinterpret_cast<char*>(&deleteFlag), sizeof(bool));	// delete flag
}

void DBMS::appendAreaRecordWithAlphaCorrection(std::ofstream& primary, std::ofstream& indexes, AreaRecord record, uint32_t& pageCounter)
{
	// Skip empty records
	if (record.key == 0)
	{
		return;
	}

	// Add empty page if it's the very first record
	if (pageCounter == 0)
	{
		indexes.write(reinterpret_cast<char*>(&record.key), sizeof(uint32_t));
		
		this->appendAreaRecord(primary, record);
		pageCounter++;

		auto cursor = primary.tellp();
		for(size_t i = 1; i < this->blockingFactor; ++i)
		{
			AreaRecord emptyRecord = AreaRecord();
			this->appendAreaRecord(primary, emptyRecord);
		}
		primary.seekp(cursor);

		return;
	}

	// Otherwise just append record OR if exceeds alpha then go to the new page
	const double currAlpha = pageCounter / (double)this->blockingFactor;
	if (currAlpha < alpha)
	{
		this->appendAreaRecord(primary, record);
		pageCounter++;
	}
	else
	{
		for (size_t i = pageCounter; i < this->blockingFactor; ++i)
		{
			this->moveCursorToTheNextAreaRecord(primary);
		}
		pageCounter = 0;
		this->appendAreaRecordWithAlphaCorrection(primary, indexes, record, pageCounter);
	}
}

void DBMS::appendAreaRecordsFromOverflow(std::ofstream& primary, std::ifstream& overflow, std::ofstream& indexes, uint32_t startIndex, uint32_t& pageCounter)
{
	while (startIndex != 0)
	{
		AreaRecord areaRecord = this->getAreaRecord(overflow, startIndex);
		this->appendAreaRecordWithAlphaCorrection(primary, indexes, areaRecord, pageCounter);

		startIndex = areaRecord.pointer;
	}
}

void DBMS::setAreaRecord(std::ofstream& file, AreaRecord record, uint32_t index)
{
	const std::streampos currPos = file.tellp();
	file.seekp(index * mainRecordSize, file.beg);

	this->appendAreaRecord(file, record);

	file.seekp(currPos);
}


uint32_t DBMS::getDiskPage(uint32_t key)
{
	uint32_t pageNo = this->binarySearchPage(key);

	this->diskOperations++;
	std::ifstream file = std::ifstream(this->area.primary, std::ifstream::binary);
	file.seekg((pageNo - 1) * mainRecordSize * blockingFactor, file.beg);

	for (size_t i = 0; i < blockingFactor; ++i)
	{
		this->diskPage[i] = this->getAreaRecord(file);
	}
	file.close();

	return pageNo;
}

void DBMS::setDiskPage(uint32_t pageNo)
{
	this->diskOperations++;
	std::ofstream file = std::ofstream(this->area.primary, std::ofstream::binary | std::ofstream::in);

	for (size_t i = 0; i < blockingFactor; ++i)
	{
		this->setAreaRecord(file, this->diskPage[i], (pageNo - 1) * blockingFactor + i);
	}
	file.close();
}


void DBMS::setDeleteFlagInOverflow(uint32_t key, bool deleteFlag)
{

}


uint32_t DBMS::insertToOverflow(uint32_t key, Record record, uint32_t pointer)
{
	const size_t offset = sizeof(uint32_t) + 2 * sizeof(double);
	uint32_t lastPointer = 0;

	// Trace to the last pointer
	if (pointer != 0)
	{
		this->diskOperations++;
		std::ifstream input = std::ifstream(this->area.overflow, std::ifstream::binary);

		while (pointer != 0)
		{
			input.seekg(offset + (pointer - 1) * mainRecordSize, input.beg);

			lastPointer = pointer;
			input.read(reinterpret_cast<char*>(&pointer), sizeof(uint32_t));
		}
		input.close();
	}

	this->diskOperations++;
	std::ofstream output = std::ofstream(this->area.overflow, std::ofstream::binary | std::ofstream::app);

	// Insert new record to overflow
	AreaRecord areaRecord = AreaRecord(key, record, 0, false);
	this->appendAreaRecord(output, areaRecord);

	this->area.length.overflow = this->getFileLength(output);
	uint32_t newRecordLocation = this->area.length.overflow / this->mainRecordSize;

	// If there was a sequence of pointers in overflow area
	if (lastPointer != 0)
	{
		// Update last record from pointing sequence
		output.seekp((lastPointer - 1) * mainRecordSize + offset, output.beg);
		output.write(reinterpret_cast<char*>(&newRecordLocation), sizeof(uint32_t));
		output.seekp(0, output.beg);

		output.close();
		return 0;	// 0 means that the pointer to the last record is already written
	}

	output.close();
	return newRecordLocation;
}

void DBMS::insertToBasePointer(uint32_t key, Record record)
{
	if (this->basePointer == 0)
	{
		this->basePointer = this->insertToOverflow(key, record, 0);
	}
	else
	{
		this->insertToOverflow(key, record, this->basePointer);
	}
}

void DBMS::insertToPrimary(uint32_t key, Record record)
{
	uint32_t pageNo = this->getDiskPage(key);

	bool isInserted = false;
	size_t i;

	for (i = 0; i < blockingFactor && !isInserted; ++i)
	{
		// Insert if there's space
		if (this->diskPage[i].key == 0)
		{
			this->diskPage[i] = AreaRecord(key, record, 0, false);
			isInserted = true;
		}

		// Break if cannot fit in the record
		else if (this->diskPage[i].key > key)
		{
			break;
		}
	}

	if (!isInserted)
	{
		if (i == 0)
		{
			this->insertToBasePointer(key, record);
		}
		else
		{
			const uint32_t pointerToOverflow = this->insertToOverflow(key, record, this->diskPage[i - 1].pointer);
			if (pointerToOverflow != 0)
			{
				this->diskPage[i - 1].pointer = pointerToOverflow;
			}
		}
	}

	this->setDiskPage(pageNo);

}


void DBMS::reorganize()
{
	if (rename(this->area.index.c_str(), "./index.old") != 0)
	{
		std::cout << "Cannot backup INDEX file." << std::endl;
		return;
	}

	if (rename(this->area.primary.c_str(), "./primary.old") != 0)
	{
		std::cout << "Cannot backup PRIMARY file." << std::endl;
		return;
	}

	if(rename(this->area.overflow.c_str(), "./overflow.old") != 0)
	{
		std::cout << "Cannot backup OVERFLOW file." << std::endl;
		return;
	}

	uint32_t pageCounter = 0;

	this->diskOperations++;
	std::ofstream newIndexes = std::ofstream(this->area.index, std::ofstream::binary | std::ofstream::app);

	this->diskOperations++;
	std::ofstream newPrimary = std::ofstream(this->area.primary, std::ofstream::binary);

	this->diskOperations++;
	std::ifstream oldOverflow = std::ifstream("./overflow.old", std::ifstream::binary);

	this->diskOperations++;
	std::ifstream oldPrimary = std::ifstream("./primary.old", std::ifstream::binary);

	// Push to primary if base pointer is in use
	if (this->basePointer != 0)
	{
		this->appendAreaRecordsFromOverflow(newPrimary, oldOverflow, newIndexes, this->basePointer, pageCounter);
		this->basePointer = 0;
	}

	// Reorganize rest of the records
	while (oldPrimary.peek() != EOF)
	{
		AreaRecord areaRecord = this->getAreaRecord(oldPrimary);
		uint32_t pointer = areaRecord.pointer;
		areaRecord.pointer = 0;
	
		this->appendAreaRecordWithAlphaCorrection(newPrimary, newIndexes, areaRecord, pageCounter);
	
		if (pointer != 0) {
			this->appendAreaRecordsFromOverflow(newPrimary, oldOverflow, newIndexes, pointer, pageCounter);
		}
	}

	// Only create these files
	std::ofstream oldIndexes = std::ofstream("./index.old", std::ofstream::binary | std::ofstream::app);
	std::ofstream newOverflow = std::ofstream(this->area.overflow, std::ofstream::binary | std::ofstream::app);

	this->area.length.index = this->getFileLength(newIndexes);
	this->area.length.primary = this->getFileLength(newPrimary);
	this->area.length.overflow = this->getFileLength(newOverflow);

	newIndexes.close();
	newPrimary.close();
	newOverflow.close();
	oldIndexes.close();
	oldPrimary.close();
	oldOverflow.close();

	std::remove("./index.old");
	std::remove("./primary.old");
	std::remove("./overflow.old");
}


DBMS::DBMS(uint32_t blockingFactor, double alpha, double maxOverflowOccupation)
{
	this->blockingFactor = blockingFactor;
	this->basePointer = 0;
	this->overflowPointer = 0;
	this->diskOperations = 0;
	this->alpha = alpha;
	this->maxOverflowOccupation = maxOverflowOccupation;
	this->diskPage = new AreaRecord[blockingFactor];

	std::ofstream file = std::ofstream(this->area.primary, std::ofstream::app | std::ofstream::binary);
	this->area.length.primary = this->getFileLength(file);
	file.close();

	file = std::ofstream(this->area.index, std::ofstream::app | std::ofstream::binary);
	this->area.length.index = this->getFileLength(file);
	file.close();

	file = std::ofstream(this->area.overflow, std::ofstream::app | std::ofstream::binary);
	this->area.length.overflow = this->getFileLength(file);
	file.close();
}


void DBMS::update(bool forceUpdate)
{
	if (forceUpdate)
	{
		this->reorganize();
	}

	if (this->area.length.primary == 0 || this->area.length.overflow / (double)this->area.length.primary >= maxOverflowOccupation)
	{
		this->reorganize();
	}
}

void DBMS::insert(uint32_t key, Record record)
{
	if (this->area.length.primary == 0)
	{
		this->insertToBasePointer(key, record);
	}
	else
	{
		this->insertToPrimary(key, record);
	}

	this->update();
}

void DBMS::remove(uint32_t key)
{

}

void DBMS::read(uint32_t key)
{
	this->getDiskPage(key);

	bool keyFound = false;
	size_t i;

	for (i = 0; i < blockingFactor && !keyFound; ++i)
	{
		// Key was found
		if (this->diskPage[i].key == key)
		{
			keyFound = true;
		}

		// If current value is too high, then key must be in overflow area
		else if (i > 0 && this->diskPage[i].key > key && this->diskPage[i - 1].pointer > 0)
		{
			
		}
	}
}


void DBMS::printIndex()
{
	std::ifstream file = std::ifstream(this->area.index, std::ifstream::binary);

	std::cout << " === Index === " << std::endl;
	std::cout << "key" << std::endl;

	const size_t length = this->getFileLength(file) / sizeof(uint32_t);

	for (size_t i = 0; i < length; ++i)
	{
		uint32_t key;
		file.read(reinterpret_cast<char*>(&key), sizeof(uint32_t));

		std::cout << key << std::endl;
	}
	std::cout << std::endl;

	file.close();
}

void DBMS::printPrimary()
{
	std::ifstream file = std::ifstream(this->area.primary, std::ifstream::binary);

	std::cout << " === Primary === " << std::endl;
	std::cout << "key\tdata\t\tpointer\tdelete" << std::endl;

	const size_t length = this->getFileLength(file) / mainRecordSize;
	for (size_t i = 0; i < length; ++i)
	{
		uint32_t key, pointer;
		double radius, height;
		bool deleteFlag;

		file.read(reinterpret_cast<char*>(&key), sizeof(uint32_t));		// key
		file.read(reinterpret_cast<char*>(&radius), sizeof(double));	// data: radius
		file.read(reinterpret_cast<char*>(&height), sizeof(double));	// data: height
		file.read(reinterpret_cast<char*>(&pointer), sizeof(uint32_t));	// pointer
		file.read(reinterpret_cast<char*>(&deleteFlag), sizeof(bool));	// delete flag

		std::cout << key << "\t(" << radius << ", " << height << ")\t\t" << pointer << "\t" << deleteFlag << std::endl;
	}
	std::cout << std::endl;

	file.close();
}

void DBMS::printDiskPage()
{
	std::cout << " === DiskPage === " << std::endl;
	std::cout << "key\tdata\t\tpointer\tdelete" << std::endl;

	for (size_t i = 0; i < blockingFactor; ++i)
	{
		if (this->diskPage[i].key == 0)
		{
			std::cout << "<empty record>" << std::endl;
		}
		else
		{
			std::cout <<
				this->diskPage[i].key << "\t(" <<
				this->diskPage[i].data.getInput().first << ", " <<
				this->diskPage[i].data.getInput().second << ")\t\t" <<
				this->diskPage[i].pointer << "\t" <<
				this->diskPage[i].deleteFlag <<
				std::endl;
		}
	}
	std::cout << std::endl;
}

void DBMS::printOverflow()
{
	std::ifstream file = std::ifstream(this->area.overflow, std::ifstream::binary);

	std::cout << " === Overflow === " << std::endl;
	std::cout << "key\tdata\t\tpointer\tdelete" << std::endl;

	const size_t length = this->getFileLength(file) / mainRecordSize;
	for (size_t i = 0; i < length; ++i)
	{
		uint32_t key, pointer;
		double radius, height;
		bool deleteFlag;

		file.read(reinterpret_cast<char*>(&key), sizeof(uint32_t));		// key
		file.read(reinterpret_cast<char*>(&radius), sizeof(double));	// data: radius
		file.read(reinterpret_cast<char*>(&height), sizeof(double));	// data: height
		file.read(reinterpret_cast<char*>(&pointer), sizeof(uint32_t));	// pointer
		file.read(reinterpret_cast<char*>(&deleteFlag), sizeof(bool));	// delete flag

		std::cout << key << "\t(" << radius << ", " << height << ")\t\t" << pointer << "\t" << deleteFlag << std::endl;
	}
	std::cout << std::endl;

	file.close();
}

void DBMS::printAll()
{
	this->printIndex();
	this->printPrimary();
	this->printOverflow();

	std::cout << std::endl;
}

DBMS::~DBMS()
{
	delete[] this->diskPage;
}