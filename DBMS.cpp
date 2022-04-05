// ReSharper disable CppClangTidyClangDiagnosticShorten64To32
// ReSharper disable CppRedundantCastExpression
#include "DBMS.h"
#include <fstream>

#pragma warning( once : 4996 )

size_t Dbms::GetFileLength(std::ifstream& file)
{
	const std::streampos currentPosition = file.tellg();

	file.seekg(0, std::ifstream::end);
	const std::streampos length = file.tellg();

	file.seekg(currentPosition, std::ifstream::beg);
	return static_cast<size_t>(length);
}

size_t Dbms::GetFileLength(std::ofstream& file)
{
	const std::streampos currentPosition = file.tellp();

	file.seekp(0, std::ofstream::end);
	const std::streampos length = file.tellp();

	file.seekp(currentPosition, std::ofstream::beg);
	return static_cast<size_t>(length);
}

void Dbms::UpdateLengthData()
{
	const std::string filename[3] = {this->area.primary, this->area.overflow, this->area.index};
	size_t* lengthData[3] = {&this->area.length.primary, &this->area.length.overflow, &this->area.length.index};

	for (size_t i = 0; i < 3; ++i)
	{
		auto file = std::ifstream(filename[i], std::ifstream::binary);
		*lengthData[i] = this->GetFileLength(file);
		file.close();
	}
}

void Dbms::BackupBasePointer()
{
	this->diskOperations++;
	auto file = std::ofstream("basePointer.bin", std::ofstream::binary | std::ofstream::trunc);
	file.write(reinterpret_cast<char*>(&this->basePointer), sizeof(uint32_t));
	file.close();
}

void Dbms::RecreateAreas(const bool backup) const
{
	if (backup)
	{
		std::string filenamePrefixes[3] = {"./primary", "./overflow", "./index"};
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


uint32_t Dbms::GetIndexRecord(std::ifstream& file, const uint32_t index) const
{
	uint32_t record;

	file.seekg(index, std::ifstream::beg);
	file.read(reinterpret_cast<char*>(&record), indexRecordSize);

	return record;
}

uint32_t Dbms::BinarySearchPage(const uint32_t key)
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
		const uint32_t first = this->GetIndexRecord(file, pointer * indexPageSize);

		// Last page record
		bool fullPageWasRead = true;
		if ((pointer + 1) * indexPageSize - indexRecordSize > this->area.length.index)
		{
			last = this->GetIndexRecord(file, this->area.length.index - indexRecordSize);
			fullPageWasRead = false;
		}
		else
		{
			last = this->GetIndexRecord(file, (pointer + 1) * indexPageSize - indexRecordSize);
		}

		if (first <= key && last >= key || !fullPageWasRead || pointer == 0 && first > key)
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


AreaRecord Dbms::GetAreaRecord(std::ifstream& file)
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

AreaRecord Dbms::GetAreaRecord(std::ifstream& file, const uint32_t index) const
{
	const std::streampos currPos = file.tellg();

	file.seekg(index * mainRecordSize, std::ifstream::beg);
	AreaRecord areaRecord = this->GetAreaRecord(file);

	file.seekg(currPos);

	return areaRecord;
}

void Dbms::AppendAreaRecord(std::ofstream& file, AreaRecord record)
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

void Dbms::SetAreaRecord(std::ofstream& file, const AreaRecord record, const uint32_t index) const
{
	file.seekp(index * mainRecordSize, std::ofstream::beg);
	this->AppendAreaRecord(file, record);
}

bool Dbms::IsNextRecordOnCurrentPage(const uint32_t& pageAnchor, const uint32_t& pointerToNextRecord) const
{
	return pointerToNextRecord >= pageAnchor && pointerToNextRecord <= pageAnchor + blockingFactor - 1 ? true : false;
}


void Dbms::GetRawPage(const std::string& filename, const uint32_t& index, AreaRecord* dest)
{
	this->diskOperations++;
	auto file = std::ifstream(filename, std::ifstream::binary);
	const auto maxPosition = this->GetFileLength(file) / mainRecordSize;

	for (size_t i = 0; i < blockingFactor && index + i < maxPosition; ++i)
	{
		dest[i] = this->GetAreaRecord(file, index + i);
	}
	file.close();
}

void Dbms::AppendRawPage(const std::string& filename, const AreaRecord* src)
{
	this->diskOperations++;
	auto file = std::ofstream(filename, std::ofstream::binary | std::ofstream::app);

	for (size_t i = 0; i < blockingFactor; ++i)
	{
		this->AppendAreaRecord(file, src[i]);
	}
	file.close();
}

void Dbms::UpdateRawPage(const std::string& string, const AreaRecord* auxPage, const uint32_t anchor)
{
	this->diskOperations++;
	auto file = std::ofstream(string, std::ofstream::binary | std::ofstream::in);

	for (size_t i = 0; i < blockingFactor; ++i)
	{
		if (auxPage[i].key != 0)
		{
			this->SetAreaRecord(file, auxPage[i], anchor + i);
		}
	}
}

void Dbms::AppendPageWithAlphaCorrection(uint32_t& currentOccupation)
{
	const auto auxPage = new AreaRecord[blockingFactor];
	const auto limit = static_cast<uint32_t>(std::floor(alpha * blockingFactor));
	uint32_t diskPageIndex = 0;
	bool pageIsEmpty = true;

	while (diskPageIndex < blockingFactor)
	{
		// Skip empty keys & deleted keys
		if (this->diskPage[diskPageIndex].key == 0 || this->diskPage[diskPageIndex].deleteFlag == true)
		{
			diskPageIndex++;
			continue;
		}
		pageIsEmpty = false;

		if (currentOccupation < limit)
		{
			auxPage[currentOccupation] = this->diskPage[diskPageIndex++];

			// If new page => add key to indexes
			if (currentOccupation == 0)
			{
				this->diskOperations++;
				auto indexes = std::ofstream(this->area.index, std::ofstream::binary | std::ofstream::app);
				indexes.write(reinterpret_cast<char*>(&auxPage[0].key), sizeof(uint32_t));

				indexes.close();
			}
			currentOccupation++;
		}
		else
		{
			this->AppendRawPage(this->area.primary, auxPage);

			// Clear auxPage
			for (size_t j = 0; j < blockingFactor; ++j)
			{
				auxPage[j] = AreaRecord();
			}
			currentOccupation = 0;
		}
	}

	currentOccupation = (currentOccupation < limit) ? currentOccupation : 0;
	if (!pageIsEmpty)
	{
		if (auxPage[0].key > 0)
		{
			this->AppendRawPage(this->area.primary, auxPage);
		}
		else
		{
			this->UpdateLengthData();
			this->UpdateRawPage(this->area.primary, auxPage,
			                    (this->area.length.index - 1) / this->indexRecordSize * blockingFactor);
		}
	}
	delete[] auxPage;
}


std::pair<uint32_t, AreaRecord> Dbms::FindAreaRecordInOverflow(const uint32_t key, uint32_t pointer)
{
	const auto auxPage = new AreaRecord[blockingFactor];
	uint32_t selectedIndex = 0, anchor = pointer;

	this->GetRawPage(this->area.overflow, pointer - 1, auxPage);

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

		if (this->IsNextRecordOnCurrentPage(anchor, pointer))
		{
			selectedIndex = pointer - anchor;
		}
		else
		{
			this->GetRawPage(this->area.overflow, pointer - 1, auxPage);
			anchor = pointer;
			selectedIndex = 0;
		}
	}

	delete[] auxPage;
	return {0, AreaRecord(0, Record(0, 0), 0, false)};
}

AreaRecord Dbms::FindAreaRecord(const uint32_t key)
{
	this->GetDiskPage(key);

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
			return this->FindAreaRecordInOverflow(key, this->basePointer).second;
		}

		// If there's a link to overflow area in the middle of page
		if (i > 0 && this->diskPage[i].key > key && this->diskPage[i - 1].pointer > 0)
		{
			return this->FindAreaRecordInOverflow(key, this->diskPage[i - 1].pointer).second;
		}
	}

	// If there's a link to overflow on the very last record
	if (this->diskPage[blockingFactor - 1].pointer > 0)
	{
		return this->FindAreaRecordInOverflow(key, this->diskPage[blockingFactor - 1].pointer).second;
	}

	return {0, Record(0, 0), 0, false};
}

bool Dbms::UpdateAreaRecordInOverflow(const uint32_t key, const Record data, const uint32_t startPointer)
{
	auto result = this->FindAreaRecordInOverflow(key, startPointer);

	if (result.first > 0)
	{
		result.second.data = data;
		result.second.deleteFlag = false;

		this->diskOperations++;
		auto file = std::ofstream(this->area.primary, std::ofstream::binary | std::ofstream::in);
		this->SetAreaRecord(file, result.second, result.first - 1);
		file.close();

		return true;
	}
	return false;
}


void Dbms::ClearDiskPage() const
{
	for (size_t i = 0; i < blockingFactor; ++i)
	{
		this->diskPage[i] = AreaRecord();
	}
}

uint32_t Dbms::GetDiskPage(const uint32_t key)
{
	const uint32_t pageNo = this->BinarySearchPage(key);

	this->diskOperations++;
	auto file = std::ifstream(this->area.primary, std::ifstream::binary);
	file.seekg((pageNo - 1) * mainRecordSize * blockingFactor, std::basic_ifstream<char>::beg);

	for (size_t i = 0; i < blockingFactor; ++i)
	{
		this->diskPage[i] = this->GetAreaRecord(file);
	}
	file.close();

	return pageNo;
}

void Dbms::SetDiskPage(const uint32_t pageNo)
{
	this->diskOperations++;
	auto file = std::ofstream(this->area.primary, std::ofstream::binary | std::ofstream::in);

	for (size_t i = 0; i < blockingFactor; ++i)
	{
		this->SetAreaRecord(file, this->diskPage[i], (pageNo - 1) * blockingFactor + i);
	}
	file.close();
}


AreaRecord Dbms::SetToDeleteInOverflow(const uint32_t key, const uint32_t pointer)
{
	auto result = this->FindAreaRecordInOverflow(key, pointer);

	if (result.first > 0)
	{
		result.second.deleteFlag = true;

		this->diskOperations++;
		auto file = std::ofstream(this->area.primary, std::ofstream::binary | std::ofstream::in);
		this->SetAreaRecord(file, result.second, result.first - 1);
		file.close();
	}
	else
	{
		std::cout << "Record doesn't exist" << std::endl;
	}

	return result.second;
}


void Dbms::InsertToOverflow(uint32_t key, Record record, uint32_t& startPointer)
{
	// If pointer wasn't assigned, just append new record to overflow
	if (startPointer == 0)
	{
		// If record wasn't already existed
		if (!this->UpdateAreaRecordInOverflow(key, record, startPointer))
		{
			this->diskOperations++;
			auto output = std::ofstream(this->area.overflow, std::ofstream::binary | std::ofstream::app);

			this->AppendAreaRecord(output, AreaRecord(key, record, 0, false));
			output.close();

			startPointer = this->area.length.overflow / this->mainRecordSize + 1;
			this->UpdateLengthData();
		}

		return;
	}

	const auto auxPage = new AreaRecord[blockingFactor];
	uint32_t selectedIndex = 0, anchor = startPointer, lastPointer = startPointer, pointer = startPointer;
	bool alreadyInserted = false;

	this->GetRawPage(this->area.overflow, pointer - 1, auxPage);

	while (pointer != 0)
	{
		lastPointer = pointer;
		pointer = auxPage[selectedIndex].pointer;

		// If record already exist, just updateFileStructure it's data and turn off deleteFlag
		if (auxPage[selectedIndex].key == key)
		{
			alreadyInserted = true;

			this->diskOperations++;
			auto file = std::ofstream(this->area.overflow, std::ofstream::binary | std::ofstream::in);

			this->SetAreaRecord(file, AreaRecord(key, record, pointer, false), lastPointer - 1);
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
			this->SetAreaRecord(file,
			                    AreaRecord(key, record, this->area.length.overflow / this->mainRecordSize + 1, false),
			                    lastPointer - 1);
			file.close();
			file = std::ofstream(this->area.overflow, std::ofstream::binary | std::ofstream::app);

			// Append old record
			this->diskOperations++;
			AreaRecord areaRecord = auxPage[selectedIndex];
			this->AppendAreaRecord(file, areaRecord);

			file.close();
			break;
		}

		// If we have reached the end
		if (pointer == 0)
		{
			break;
		}

		// Update page if necessary to move to the next record
		if (this->IsNextRecordOnCurrentPage(anchor, pointer))
		{
			selectedIndex = pointer - anchor;
		}
		else
		{
			this->GetRawPage(this->area.overflow, pointer - 1, auxPage);
			anchor = pointer;
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
		this->SetAreaRecord(file, recordToUpdate, lastPointer - 1);

		file.close();
		file = std::ofstream(this->area.overflow, std::ofstream::binary | std::ofstream::app);

		// Append new record
		this->diskOperations++;
		this->AppendAreaRecord(file, AreaRecord(key, record, 0, false));

		file.close();
	}
	delete[] auxPage;
	this->UpdateLengthData();
}

void Dbms::InsertToBasePointer(const uint32_t key, const Record record)
{
	this->InsertToOverflow(key, record, this->basePointer);
	this->BackupBasePointer();
}

void Dbms::InsertToPrimary(const uint32_t key, const Record record)
{
	const uint32_t pageNo = this->GetDiskPage(key);

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
			this->InsertToBasePointer(key, record);
		}
		else
		{
			this->InsertToOverflow(key, record, this->diskPage[i - 1].pointer);
		}
	}

	this->SetDiskPage(pageNo);
}


void Dbms::FillRecordsFromOverflow(size_t& pointer, size_t& index)
{
	const auto auxPage = new AreaRecord[blockingFactor];
	uint32_t overflowIndex = 0, anchor = pointer;

	this->GetRawPage("./overflow.old", pointer - 1, auxPage);

	while (index < blockingFactor && pointer != 0)
	{
		pointer = auxPage[overflowIndex].pointer;

		this->diskPage[index] = auxPage[overflowIndex];
		this->diskPage[index].pointer = 0;
		this->diskPage[index].deleteFlag = false;

		// Determine necessity of next disk operation
		if (this->IsNextRecordOnCurrentPage(anchor, pointer))
		{
			overflowIndex = pointer - anchor;
		}
		else
		{
			// If it's already the last record, no need to get anymore pages
			if (pointer == 0)
			{
				index++;
				break;
			}

			this->GetRawPage("./overflow.old", pointer - 1, auxPage);
			anchor = pointer;
			overflowIndex = 0;
		}

		index++;
	}

	delete[] auxPage;
}

void Dbms::GetPageToReorganize(uint32_t& lastPosition, uint32_t& lastPointer)
{
	this->ClearDiskPage();

	const auto auxPage = new AreaRecord[blockingFactor];
	bool primaryLoaded = false;

	size_t primaryIterator = 0;
	for (size_t i = 0; i < blockingFactor; ++i)
	{
		// The next record is in overflow
		if (lastPointer > 0)
		{
			this->FillRecordsFromOverflow(lastPointer, i);
			if (i >= blockingFactor || this->area.length.primary == 0)
			{
				break;
			}
		}

		// Load primary page
		if (!primaryLoaded)
		{
			this->GetRawPage("./primary.old", lastPosition - 1, auxPage);
			primaryLoaded = true;
		}

		if (primaryIterator < blockingFactor)
		{
			this->diskPage[i].key = auxPage[primaryIterator].key;
			this->diskPage[i].data = auxPage[primaryIterator].data;
			this->diskPage[i].deleteFlag = auxPage[primaryIterator].deleteFlag;
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

void Dbms::Reorganize()
{
	std::cout << "Reorganizing!" << std::endl;
	this->RecreateAreas(true);

	uint32_t lastPosition = 1;
	uint32_t lastPointer = this->basePointer;
	uint32_t currPageOccupancy = 0;

	const uint32_t maxPosition = this->area.length.primary / mainRecordSize;
	while (lastPosition < maxPosition || lastPointer > 0)
	{
		this->GetPageToReorganize(lastPosition, lastPointer);
		this->AppendPageWithAlphaCorrection(currPageOccupancy);
	}

	this->basePointer = 0;
	this->BackupBasePointer();
	this->UpdateLengthData();
}


Dbms::Dbms(const uint32_t blockingFactor, const double alpha, const double maxOverflowOccupation) :
	diskPage(new AreaRecord[blockingFactor]), blockingFactor(blockingFactor), alpha(alpha),
	maxOverflowOccupation(maxOverflowOccupation)
{
	this->RecreateAreas(false);

	// Init base pointer first time if necessary
	auto writeBasePointerFile = std::ofstream("basePointer.bin", std::ofstream::binary | std::ofstream::app);
	if (this->GetFileLength(writeBasePointerFile) == 0)
	{
		writeBasePointerFile.write(reinterpret_cast<char*>(&this->basePointer), sizeof(uint32_t));
	}
	writeBasePointerFile.close();

	auto readBasePointerFile = std::ifstream("basePointer.bin", std::ifstream::binary);
	readBasePointerFile.read(reinterpret_cast<char*>(&this->basePointer), sizeof(uint32_t));
	readBasePointerFile.close();

	this->UpdateLengthData();
}


void Dbms::UpdateFileStructure(const bool forceUpdate)
{
	if (this->area.length.primary == 0 ||
		this->area.length.overflow / static_cast<double>(this->area.length.primary) >= maxOverflowOccupation ||
		forceUpdate)
	{
		this->Reorganize();
	}
}

void Dbms::Insert(const uint32_t key, const Record record)
{
	if (key == 0)
	{
		std::cout << "Cannot insert an empty key!" << std::endl;
		return;
	}

	if (this->area.length.primary == 0)
	{
		this->InsertToBasePointer(key, record);
	}
	else
	{
		this->InsertToPrimary(key, record);
	}

	this->UpdateFileStructure();
}

void Dbms::UpdateRecord(const uint32_t key, const Record record)
{
	const auto pageNo = this->GetDiskPage(key);

	bool isInserted = false;
	size_t i;

	for (i = 0; i < blockingFactor && !isInserted; ++i)
	{
		// Insert if there's space
		if (this->diskPage[i].key == 0)
		{
			std::cout << "Record doesn't exist!" << std::endl;
			return;
		}

		// Update data on the same key
		if (this->diskPage[i].key == key)
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
		const auto recordInOverflow = this->FindAreaRecordInOverflow(
			key, (i == 0) ? this->basePointer : this->diskPage[i - 1].pointer);
		if (recordInOverflow.first != 0)
		{
			this->UpdateAreaRecordInOverflow(key, record, recordInOverflow.first);
		}

		std::cout << "Record doesn't exist!" << std::endl;
	}
	else
	{
		// We don't need to updateRecord the pointer in primary, no need to set the page
		this->SetDiskPage(pageNo);
	}
}

void Dbms::UpdateKey(const uint32_t oldKey, const uint32_t newKey)
{
	if (oldKey == newKey)
	{
		return;
	}

	const auto areaRecord = this->Remove(oldKey);
	if (areaRecord.key > 0)
	{
		this->Insert(newKey, areaRecord.data);
	}
	else
	{
		std::cout << "Cannot update not existing record!" << std::endl;
	}
}

AreaRecord Dbms::Remove(const uint32_t key)
{
	const uint32_t pageNo = this->GetDiskPage(key);
	auto areaRecord = AreaRecord();

	for (size_t i = 0; i < blockingFactor; ++i)
	{
		// If there's space => record simply doesn't exist
		if (this->diskPage[i].key == 0)
		{
			std::cout << "Record doesn't exist!" << std::endl;
			return areaRecord;
		}

		// Select record to delete
		if (this->diskPage[i].key == key)
		{
			areaRecord = this->diskPage[i];
			this->diskPage[i].deleteFlag = true;
			break;
		}

		if (this->diskPage[i].key > key)
		{
			if (i == 0)
			{
				areaRecord = this->SetToDeleteInOverflow(key, this->basePointer);
			}
			else
			{
				areaRecord = this->SetToDeleteInOverflow(key, this->diskPage[i].pointer);
			}

			return areaRecord;
		}
	}

	this->SetDiskPage(pageNo);
	return areaRecord;
}

void Dbms::Read(const uint32_t key)
{
	auto areaRecord = this->FindAreaRecord(key);
	if (areaRecord.key != 0)
	{
		std::cout << "Data: (" << areaRecord.data.getInput().first << ", " << areaRecord.data.getInput().second << ")"
			<< std::endl;
		std::cout << "Pointer: " << areaRecord.pointer << std::endl;
		std::cout << "DeleteFlag: " << areaRecord.deleteFlag << std::endl;
	}
	else
	{
		std::cout << "Requested record with key " << key << " doesn't exist!" << std::endl;
	}
}


void Dbms::PrintIndex() const
{
	auto file = std::ifstream(this->area.index, std::ifstream::binary);

	std::cout << " === Index === " << std::endl;
	std::cout << "key" << std::endl;

	const size_t length = this->GetFileLength(file) / sizeof(uint32_t);

	for (size_t i = 0; i < length; ++i)
	{
		uint32_t key;
		file.read(reinterpret_cast<char*>(&key), sizeof(uint32_t));

		std::cout << key << std::endl;
	}
	std::cout << std::endl;

	file.close();
}

void Dbms::PrintPrimary() const
{
	auto file = std::ifstream(this->area.primary, std::ifstream::binary);

	std::cout << " === Primary === " << std::endl;
	std::cout << "key\tdata\t\tpointer\tdelete" << std::endl;

	const size_t length = this->GetFileLength(file) / mainRecordSize;
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

void Dbms::PrintDiskPage() const
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

void Dbms::PrintOverflow() const
{
	auto file = std::ifstream(this->area.overflow, std::ifstream::binary);

	std::cout << " === Overflow === " << std::endl;
	std::cout << "key\tdata\t\tpointer\tdelete" << std::endl;

	const size_t length = this->GetFileLength(file) / mainRecordSize;
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

void Dbms::PrintAll() const
{
	std::cout << std::endl;

	this->PrintIndex();
	this->PrintPrimary();
	this->PrintOverflow();
}

void Dbms::PrintDiskOperations(const bool resetCounter)
{
	std::cout << "Disk operations: " << this->diskOperations << std::endl;
	if (resetCounter)
	{
		this->diskOperations = 0;
	}
}

Dbms::~Dbms()
{
	delete[] this->diskPage;
}
