// ReSharper disable CppClangTidyClangDiagnosticShorten64To32
// ReSharper disable CppRedundantCastExpression
#include "DBMS.h"
#include <fstream>

#pragma warning( once : 4996 )

size_t Dbms::getFileLength(std::ifstream& file)
{
	const std::streampos currPos = file.tellg();

	file.seekg(0, std::ifstream::end);
	const std::streampos length = file.tellg();

	file.seekg(currPos, std::ifstream::beg);
	return static_cast<size_t>(length);
}

size_t Dbms::getFileLength(std::ofstream& file)
{
	const std::streampos currPos = file.tellp();

	file.seekp(0, std::ofstream::end);
	const std::streampos length = file.tellp();

	file.seekp(currPos, std::ofstream::beg);
	return static_cast<size_t>(length);
}

void Dbms::updateLengthData()
{
	const std::string filename[3] = {this->area.primary, this->area.overflow, this->area.index};
	size_t* lengthData[3] = {&this->area.length.primary, &this->area.length.overflow, &this->area.length.index};

	for (size_t i = 0; i < 3; ++i)
	{
		auto file = std::ifstream(filename[i], std::ifstream::binary);
		*lengthData[i] = this->getFileLength(file);
		file.close();
	}
}

void Dbms::backupBasePointer()
{
	this->diskOperations++;
	auto file = std::ofstream("basePointer.bin", std::ofstream::binary | std::ofstream::trunc);
	file.write(reinterpret_cast<char*>(&this->basePointer), sizeof(uint32_t));
	file.close();
}

void Dbms::recreateAreas(const bool backup) const
{
	if (backup)
	{
		std::string filenamePrefixes[3] = { "./primary", "./overflow", "./index" };
		for (const auto& i : filenamePrefixes)
		{
			std::remove((i + ".old").c_str());
			if (std::rename((i + ".bin").c_str(), (i + ".old").c_str()) != 0)
			{
				std::cout << "Backup failed for " << i << std::endl;
				return;
			}
		}
	}

	const std::string filename[3] = {this->area.primary, this->area.overflow, this->area.index};
	for (const auto& i : filename)
	{
		auto file = std::ifstream(i, std::ifstream::binary | std::ifstream::app);
		file.close();
	}
}


uint32_t Dbms::getIndexRecord(std::ifstream& file, const uint32_t index) const
{
	uint32_t record;

	file.seekg(index, std::ifstream::beg);
	file.read(reinterpret_cast<char*>(&record), indexRecordSize);

	return record;
}

uint32_t Dbms::binarySearchPage(const uint32_t key)
{
	const size_t indexPageSize = blockingFactor * mainRecordSize / indexRecordSize * indexRecordSize;
	const size_t pages = this->area.length.index / indexPageSize + 1;
	uint32_t left = 0, right = pages, pointer = 0;

	const auto indexPage = new uint32_t[indexPageSize];
	size_t indexPageLength = 0;

	// External binary search
	bool found = false;
	while (!found)
	{
		uint32_t last;

		this->diskOperations++;
		auto file = std::ifstream(this->area.index, std::ifstream::binary);

		pointer = (left + right) / 2;

		// First page record
		const uint32_t first = this->getIndexRecord(file, pointer * indexPageSize);

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

		if (first <= key && last >= key || !fullPageWasRead)
		{
			file.seekg(pointer * indexPageSize, std::basic_ifstream<char>::beg);
			do
			{
				file.read(reinterpret_cast<char*>(&indexPage[indexPageLength]), indexRecordSize);
			}
			while (indexPageLength < indexPageSize && indexPage[indexPageLength++] != last);

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

			right = right - left != 1 ? right : right - 1; // fix stuck on 0.5 issue
		}
	}
	delete[] indexPage;

	return pointer + 1; // returns position, not index
}


AreaRecord Dbms::getAreaRecord(std::ifstream& file)
{
	uint32_t key, pointer;
	double radius, height;
	bool deleteFlag;

	file.read(reinterpret_cast<char*>(&key), sizeof(uint32_t));
	file.read(reinterpret_cast<char*>(&radius), sizeof(double));
	file.read(reinterpret_cast<char*>(&height), sizeof(double));
	file.read(reinterpret_cast<char*>(&pointer), sizeof(uint32_t));
	file.read(reinterpret_cast<char*>(&deleteFlag), sizeof(bool));

	return {key, Record(radius, height), pointer, deleteFlag};
}

AreaRecord Dbms::getAreaRecord(std::ifstream& file, const uint32_t index) const
{
	const std::streampos currPos = file.tellg();

	file.seekg(index * mainRecordSize, std::ifstream::beg);
	AreaRecord areaRecord = this->getAreaRecord(file);

	file.seekg(currPos);

	return areaRecord;
}

void Dbms::appendAreaRecord(std::ofstream& file, AreaRecord record)
{
	uint32_t key = record.key, pointer = record.pointer;
	double radius = record.data.getInput().first;
	double height = record.data.getInput().second;
	bool deleteFlag = record.deleteFlag;

	file.write(reinterpret_cast<char*>(&key), sizeof(uint32_t));
	file.write(reinterpret_cast<char*>(&radius), sizeof(double));
	file.write(reinterpret_cast<char*>(&height), sizeof(double));
	file.write(reinterpret_cast<char*>(&pointer), sizeof(uint32_t));
	file.write(reinterpret_cast<char*>(&deleteFlag), sizeof(bool));
}

void Dbms::setAreaRecord(std::ofstream& file, const AreaRecord record, const uint32_t index) const
{
	file.seekp(index * mainRecordSize, std::ofstream::beg);
	this->appendAreaRecord(file, record);
}

bool Dbms::isNextRecordOnCurrentPage(const uint32_t& pageAnchor, const uint32_t& pointerToNextRecord) const
{
	return pointerToNextRecord >= pageAnchor && pointerToNextRecord <= pageAnchor + blockingFactor - 1 ? true : false;
}


void Dbms::getRawPage(const std::string& filename, const uint32_t& index, AreaRecord* dest)
{
	this->diskOperations++;
	auto file = std::ifstream(filename, std::ifstream::binary);
	const auto maxPosition = this->getFileLength(file) / mainRecordSize;

	for (size_t i = 0; i < blockingFactor && index + i < maxPosition; ++i)
	{
		dest[i] = this->getAreaRecord(file, index + i);
	}
	file.close();
}

void Dbms::appendRawPage(const std::string& filename, const AreaRecord* src)
{
	this->diskOperations++;
	auto file = std::ofstream(filename, std::ofstream::binary | std::ofstream::app);

	for (size_t i = 0; i < blockingFactor; ++i)
	{
		this->appendAreaRecord(file, src[i]);
	}
	file.close();
}

void Dbms::appendPageWithAlphaCorrection(uint32_t& currOccupation)
{
	const auto auxPage = new AreaRecord[blockingFactor];
	const auto limit = static_cast<uint32_t>(std::floor(alpha * blockingFactor));
	uint32_t diskPageIndex = 0;
	bool pageIsEmpty = true;

	while (diskPageIndex < blockingFactor)
	{
		// Skip empty keys
		if (this->diskPage[diskPageIndex].key == 0)
		{
			diskPageIndex++;
			continue;
		}
		pageIsEmpty = false;

		if (currOccupation < limit)
		{
			auxPage[currOccupation] = this->diskPage[diskPageIndex++];

			// If new page => add key to indexes
			if (currOccupation == 0)
			{
				this->diskOperations++;
				auto indexes = std::ofstream(this->area.index, std::ofstream::binary | std::ofstream::app);
				indexes.write(reinterpret_cast<char*>(&auxPage[0].key), sizeof(uint32_t));

				indexes.close();
			}
			currOccupation++;
		}
		else
		{
			this->appendRawPage(this->area.primary, auxPage);

			// Clear auxPage
			for (size_t j = 0; j < blockingFactor; ++j)
			{
				auxPage[j] = AreaRecord();
			}
			currOccupation = 0;
		}
	}
	
	currOccupation = (currOccupation < limit) ? currOccupation : 0;
	if (!pageIsEmpty)
	{
		this->appendRawPage(this->area.primary, auxPage);
	}
	delete[] auxPage;
}


std::pair<uint32_t, AreaRecord> Dbms::findAreaRecordInOverflow(const uint32_t key, uint32_t pointer)
{
	const auto auxPage = new AreaRecord[blockingFactor];
	uint32_t selectedIndex = 0, anchor = pointer;

	this->getRawPage(this->area.overflow, pointer - 1, auxPage);

	while (pointer != 0)
	{
		const uint32_t lastPointer = pointer;
		pointer = auxPage[selectedIndex].pointer;

		if (auxPage[selectedIndex].key == key)
		{
			AreaRecord areaRecord = auxPage[selectedIndex];
			delete[] auxPage;

			return {lastPointer, areaRecord};
		}

		if (this->isNextRecordOnCurrentPage(anchor, pointer))
		{
			selectedIndex = (pointer > lastPointer) ? pointer - lastPointer : lastPointer - pointer;
		}
		else
		{
			this->getRawPage(this->area.overflow, pointer - 1, auxPage);
			anchor = auxPage[0].pointer;
			selectedIndex = 0;
		}
	}

	delete[] auxPage;
	return {0, AreaRecord(0, Record(0, 0), 0, false)};
}

AreaRecord Dbms::findAreaRecord(const uint32_t key)
{
	this->getDiskPage(key);

	for (size_t i = 0; i < blockingFactor; ++i)
	{
		// Key was found
		if (this->diskPage[i].key == key)
		{
			return this->diskPage[i];
		}

		// If record can only be linked to base pointer
		if (this->diskPage[i].key > key && i == 0)
		{
			return this->findAreaRecordInOverflow(key, this->basePointer).second;
		}

		// If there's a link to overflow area in the middle of page
		if (i > 0 && this->diskPage[i].key > key && this->diskPage[i - 1].pointer > 0)
		{
			return this->findAreaRecordInOverflow(key, this->diskPage[i - 1].pointer).second;
		}
	}

	// If there's a link to overflow on the very last record
	if (this->diskPage[blockingFactor - 1].pointer > 0)
	{
		return this->findAreaRecordInOverflow(key, this->diskPage[blockingFactor - 1].pointer).second;
	}

	return {0, Record(0, 0), 0, false};
}

bool Dbms::updateAreaRecordInOverflow(const uint32_t key, const Record data, const uint32_t startPointer)
{
	auto result = this->findAreaRecordInOverflow(key, startPointer);

	if (result.first > 0)
	{
		result.second.data = data;
		result.second.deleteFlag = false;

		this->diskOperations++;
		auto file = std::ofstream(this->area.primary, std::ofstream::binary | std::ofstream::in);
		this->setAreaRecord(file, result.second, result.first - 1);
		file.close();

		return true;
	}
	return false;
}


void Dbms::clearDiskPage() const
{
	for (size_t i = 0; i < blockingFactor; ++i)
	{
		this->diskPage[i] = AreaRecord();
	}
}

uint32_t Dbms::getDiskPage(const uint32_t key)
{
	const uint32_t pageNo = this->binarySearchPage(key);

	this->diskOperations++;
	auto file = std::ifstream(this->area.primary, std::ifstream::binary);
	file.seekg((pageNo - 1) * mainRecordSize * blockingFactor, std::basic_ifstream<char>::beg);

	for (size_t i = 0; i < blockingFactor; ++i)
	{
		this->diskPage[i] = this->getAreaRecord(file);
	}
	file.close();

	return pageNo;
}

void Dbms::setDiskPage(const uint32_t pageNo)
{
	this->diskOperations++;
	auto file = std::ofstream(this->area.primary, std::ofstream::binary | std::ofstream::in);

	for (size_t i = 0; i < blockingFactor; ++i)
	{
		this->setAreaRecord(file, this->diskPage[i], (pageNo - 1) * blockingFactor + i);
	}
	file.close();
}


void Dbms::setToDeleteInOverflow(const uint32_t key, const uint32_t pointer)
{
	auto result = this->findAreaRecordInOverflow(key, pointer);

	if (result.first > 0)
	{
		result.second.deleteFlag = true;

		this->diskOperations++;
		auto file = std::ofstream(this->area.primary, std::ofstream::binary | std::ofstream::in);
		this->setAreaRecord(file, result.second, result.first - 1);
		file.close();
	}
}


void Dbms::insertToOverflow(uint32_t key, Record record, uint32_t& startPointer)
{
	// If pointer wasn't assigned, just append new record to overflow
	if (startPointer == 0)
	{
		// If record wasn't already existed
		if (!this->updateAreaRecordInOverflow(key, record, startPointer))
		{
			this->diskOperations++;
			auto output = std::ofstream(this->area.overflow, std::ofstream::binary | std::ofstream::app);

			this->appendAreaRecord(output, AreaRecord(key, record, 0, false));
			output.close();

			startPointer = this->area.length.overflow / this->mainRecordSize + 1;
			this->updateLengthData();
		}

		return;
	}

	const auto auxPage = new AreaRecord[blockingFactor];
	uint32_t selectedIndex = 0, anchor = startPointer, lastPointer = startPointer, pointer = startPointer;
	bool alreadyInserted = false;

	this->getRawPage(this->area.overflow, pointer - 1, auxPage);

	while (pointer != 0)
	{
		lastPointer = pointer;
		pointer = auxPage[selectedIndex].pointer;

		// If record already exist, just update it's data and turn off deleteFlag
		if (auxPage[selectedIndex].key == key)
		{
			alreadyInserted = true;

			this->diskOperations++;
			auto file = std::ofstream(this->area.overflow, std::ofstream::binary | std::ofstream::in);

			this->setAreaRecord(file, AreaRecord(key, record, pointer, false), lastPointer - 1);
			file.close();

			break;
		}

		// If there's greater record than the current, override current and set pointer to the new record location
		if (auxPage[selectedIndex].key > key)
		{
			alreadyInserted = true;

			auto file = std::ofstream(this->area.overflow, std::ofstream::binary | std::ofstream::in);

			// Update record with the inserted one
			this->diskOperations++;
			this->setAreaRecord(file,
								AreaRecord(key, record, this->area.length.overflow / this->mainRecordSize + 1, false),
			                    lastPointer - 1);
			file.close();
			file = std::ofstream(this->area.overflow, std::ofstream::binary | std::ofstream::app);

			// Append old record
			this->diskOperations++;
			AreaRecord areaRecord = auxPage[selectedIndex];
			this->appendAreaRecord(file, areaRecord);

			file.close();
			break;
		}

		// If we have reached the end
		if (pointer == 0)
		{
			break;
		}

		// Update page if necessary to move to the next record
		if (this->isNextRecordOnCurrentPage(anchor, pointer))
		{
			selectedIndex = (pointer > lastPointer) ? pointer - lastPointer : lastPointer - pointer;
		}
		else
		{
			this->getRawPage(this->area.overflow, pointer - 1, auxPage);
			anchor = auxPage[0].pointer;
			selectedIndex = 0;
		}
	}

	if (!alreadyInserted)
	{
		auto file = std::ofstream(this->area.overflow, std::ofstream::binary | std::ofstream::in);

		AreaRecord recordToUpdate = auxPage[selectedIndex];
		recordToUpdate.pointer = this->area.length.overflow / this->mainRecordSize + 1;

		// Set pointer to the new record
		this->diskOperations++;
		this->setAreaRecord(file, recordToUpdate,lastPointer - 1);

		file.close();
		file = std::ofstream(this->area.overflow, std::ofstream::binary | std::ofstream::app);

		// Append new record
		this->diskOperations++;
		this->appendAreaRecord(file, AreaRecord(key, record, 0, false));

		file.close();
	}
	delete[] auxPage;
	this->updateLengthData();
}

void Dbms::insertToBasePointer(const uint32_t key, const Record record)
{
	this->insertToOverflow(key, record, this->basePointer);
	this->backupBasePointer();
}

void Dbms::insertToPrimary(const uint32_t key, const Record record)
{
	const uint32_t pageNo = this->getDiskPage(key);

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

		// Update data if it's a duplicate
		else if (this->diskPage[i].key == key)
		{
			this->diskPage[i].data = record;
			this->diskPage[i].deleteFlag = false;
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
			this->insertToOverflow(key, record, this->diskPage[i - 1].pointer);
		}
	}

	this->setDiskPage(pageNo);
}


void Dbms::fillRecordsFromOverflow(size_t& pointer, size_t& index)
{
	const auto auxPage = new AreaRecord[blockingFactor];
	uint32_t overflowIndex = 0;

	bool nextRecordInPage = false;
	while (index < blockingFactor && pointer != 0)
	{
		if (!nextRecordInPage)
		{
			this->getRawPage("./overflow.old", pointer - 1, auxPage);
		}

		this->diskPage[index].key = auxPage[overflowIndex].key;
		this->diskPage[index].data = auxPage[overflowIndex].data;

		// Determine necessity of next disk operation
		if (this->isNextRecordOnCurrentPage(auxPage[index].pointer, pointer))
		{
			overflowIndex = 0;
			nextRecordInPage = false;
		}
		else
		{
			overflowIndex += auxPage[index].pointer - pointer;
			nextRecordInPage = true;
		}

		pointer = auxPage[index].pointer;
		index++;
	}

	delete[] auxPage;
}

void Dbms::getPageToReorganize(uint32_t& lastPosition, uint32_t& lastPointer)
{
	this->clearDiskPage();

	const auto auxPage = new AreaRecord[blockingFactor];
	bool primaryLoaded = false;

	size_t primaryIterator = 0;
	for (size_t i = 0; i < blockingFactor; ++i)
	{
		// The next record is in overflow
		if (lastPointer > 0)
		{
			this->fillRecordsFromOverflow(lastPointer, i);
			if (i >= blockingFactor || this->area.length.primary == 0)
			{
				break;
			}
		}

		// Load primary page
		if (!primaryLoaded)
		{
			this->getRawPage("./primary.old", lastPosition - 1, auxPage);
			primaryLoaded = true;
		}

		if (primaryIterator < blockingFactor)
		{
			this->diskPage[i].key = auxPage[primaryIterator].key;
			this->diskPage[i].data = auxPage[primaryIterator].data;
			lastPointer = auxPage[primaryIterator].pointer;
		}
		
		primaryIterator++;
		lastPosition++;
	}

	if (primaryLoaded)
	{
		delete[] auxPage;
	}
}

void Dbms::reorganize()
{
	this->recreateAreas(true);

	uint32_t lastPosition = 1;
	uint32_t lastPointer = this->basePointer;
	uint32_t currPageOccupancy = 0;

	const uint32_t maxPosition = this->area.length.primary / mainRecordSize;
	while (lastPosition < maxPosition || lastPointer > 0)
	{
		this->getPageToReorganize(lastPosition, lastPointer);
		this->appendPageWithAlphaCorrection(currPageOccupancy);
	}

	this->basePointer = 0;
	this->backupBasePointer();
	this->updateLengthData();
}


Dbms::Dbms(const uint32_t blockingFactor, const double alpha, const double maxOverflowOccupation) :
	diskPage(new AreaRecord[blockingFactor]), blockingFactor(blockingFactor), alpha(alpha), maxOverflowOccupation(maxOverflowOccupation)
{
	this->recreateAreas(false);

	// Init base pointer first time if necessary
	auto writeBasePointerFile = std::ofstream("basePointer.bin", std::ofstream::binary | std::ofstream::app);
	if (this->getFileLength(writeBasePointerFile) == 0)
	{
		writeBasePointerFile.write(reinterpret_cast<char*>(&this->basePointer), sizeof(uint32_t));
	}
	writeBasePointerFile.close();

	auto readBasePointerFile = std::ifstream("basePointer.bin", std::ifstream::binary);
	readBasePointerFile.read(reinterpret_cast<char*>(&this->basePointer), sizeof(uint32_t));
	readBasePointerFile.close();

	this->updateLengthData();
}


void Dbms::update(const bool forceUpdate)
{
	if (forceUpdate)
	{
		this->reorganize();
	}

	if (this->area.length.primary == 0 || this->area.length.overflow / static_cast<double>(this->area.length.primary) >=
		maxOverflowOccupation)
	{
		this->reorganize();
	}
}

void Dbms::insert(const uint32_t key, const Record record)
{
	if (key == 0)
	{
		std::cout << "Cannot insert an empty key!" << std::endl;
		return;
	}

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

void Dbms::remove(const uint32_t key)
{
	const uint32_t pageNo = this->getDiskPage(key);

	for (size_t i = 0; i < blockingFactor; ++i)
	{
		// Insert if there's space => record simply doesn't exist
		if (this->diskPage[i].key == 0)
		{
			return;
		}

		// Insert record is empty that means it already doesn't exist
		if (this->diskPage[i].key == key)
		{
			this->diskPage[i].deleteFlag = true;
			break;
		}

		if (this->diskPage[i].key > key)
		{
			if (i == 0)
			{
				this->setToDeleteInOverflow(key, this->basePointer);
			}
			else
			{
				this->setToDeleteInOverflow(key, this->diskPage[i].pointer);
			}

			break;
		}
	}

	this->setDiskPage(pageNo);
}

void Dbms::read(const uint32_t key)
{
	auto areaRecord = this->findAreaRecord(key);
	if (areaRecord.key != 0)
	{
		std::cout << "Data: (" << areaRecord.data.getInput().first << ", " << areaRecord.data.getInput().second << ")" << std::endl;
		std::cout << "Pointer: " << areaRecord.pointer << std::endl;
		std::cout << "DeleteFlag: " << areaRecord.deleteFlag << std::endl;
	}
	else
	{
		std::cout << "Requested record with key " << key << " doesn't exist!" << std::endl;
	}
}


void Dbms::printIndex() const
{
	auto file = std::ifstream(this->area.index, std::ifstream::binary);

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

void Dbms::printPrimary() const
{
	auto file = std::ifstream(this->area.primary, std::ifstream::binary);

	std::cout << " === Primary === " << std::endl;
	std::cout << "key\tdata\t\tpointer\tdelete" << std::endl;

	const size_t length = this->getFileLength(file) / mainRecordSize;
	for (size_t i = 0; i < length; ++i)
	{
		uint32_t key, pointer;
		double radius, height;
		bool deleteFlag;

		file.read(reinterpret_cast<char*>(&key), sizeof(uint32_t)); // key
		file.read(reinterpret_cast<char*>(&radius), sizeof(double)); // data: radius
		file.read(reinterpret_cast<char*>(&height), sizeof(double)); // data: height
		file.read(reinterpret_cast<char*>(&pointer), sizeof(uint32_t)); // pointer
		file.read(reinterpret_cast<char*>(&deleteFlag), sizeof(bool)); // delete flag

		std::cout << key << "\t(" << radius << ", " << height << ")\t\t" << pointer << "\t" << deleteFlag << std::endl;
	}
	std::cout << std::endl;

	file.close();
}

void Dbms::printDiskPage() const
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

void Dbms::printOverflow() const
{
	auto file = std::ifstream(this->area.overflow, std::ifstream::binary);

	std::cout << " === Overflow === " << std::endl;
	std::cout << "key\tdata\t\tpointer\tdelete" << std::endl;

	const size_t length = this->getFileLength(file) / mainRecordSize;
	for (size_t i = 0; i < length; ++i)
	{
		uint32_t key, pointer;
		double radius, height;
		bool deleteFlag;

		file.read(reinterpret_cast<char*>(&key), sizeof(uint32_t)); // key
		file.read(reinterpret_cast<char*>(&radius), sizeof(double)); // data: radius
		file.read(reinterpret_cast<char*>(&height), sizeof(double)); // data: height
		file.read(reinterpret_cast<char*>(&pointer), sizeof(uint32_t)); // pointer
		file.read(reinterpret_cast<char*>(&deleteFlag), sizeof(bool)); // delete flag

		std::cout << key << "\t(" << radius << ", " << height << ")\t\t" << pointer << "\t" << deleteFlag << std::endl;
	}
	std::cout << std::endl;

	file.close();
}

void Dbms::printAll() const
{
	this->printIndex();
	this->printPrimary();
	this->printOverflow();

	std::cout << std::endl;
}

Dbms::~Dbms()
{
	delete[] this->diskPage;
}
