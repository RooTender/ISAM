#include "DBMS.h"

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
		FileUtils::ChangeFileExtension(primaryArea->GetFilename(), ".old", true);
		FileUtils::ChangeFileExtension(overflowArea->GetFilename(), ".old", true);
		FileUtils::ChangeFileExtension(indexArea->GetFilename(), ".old", true);
	}

	FileUtils::CreateFile(primaryArea->GetFilename(), false);
	FileUtils::CreateFile(overflowArea->GetFilename(), false);
	FileUtils::CreateFile(indexArea->GetFilename(), false);
}

void Dbms::UpdateAreasLength() const
{
	primaryArea->UpdateLength();
	overflowArea->UpdateLength();
	indexArea->UpdateLength();
}


uint32_t Dbms::GetIndexRecord(std::ifstream& file, const uint32_t index) const
{
	uint32_t record;

	file.seekg(index, std::ifstream::beg);
	file.read(reinterpret_cast<char*>(&record), indexAreaRecordSize);

	return record;
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

	return AreaRecord(key, Record(radius, height), pointer, deleteFlag);
}

AreaRecord Dbms::GetAreaRecord(std::ifstream& file, const uint32_t index) const
{
	const std::streampos currentPosition = file.tellg();
	const unsigned selectedPage = index * fullAreaRecordSize;

	file.seekg(selectedPage, std::ifstream::beg);
	AreaRecord areaRecord = this->GetAreaRecord(file);

	file.seekg(currentPosition);

	return areaRecord;
}

void Dbms::AppendAreaRecord(std::ofstream& file, const AreaRecord& record)
{
	uint32_t key = record.GetKey(), pointer = record.GetPointer();
	double radius = record.GetRecord().getInput().first;
	double height = record.GetRecord().getInput().second;
	bool deleteFlag = record.GetDeleteFlag();

	file.write(reinterpret_cast<char*>(&key), sizeof(uint32_t));
	file.write(reinterpret_cast<char*>(&radius), sizeof(double));
	file.write(reinterpret_cast<char*>(&height), sizeof(double));
	file.write(reinterpret_cast<char*>(&pointer), sizeof(uint32_t));
	file.write(reinterpret_cast<char*>(&deleteFlag), sizeof(bool));
}

void Dbms::SetAreaRecord(std::ofstream& file, const AreaRecord record, const uint32_t index) const
{
	const unsigned selectedPage = index * fullAreaRecordSize;
	file.seekp(selectedPage, std::ofstream::beg);

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
	const auto maxPosition = FileUtils::GetFileLength(file) / fullAreaRecordSize;

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
		if (auxPage[i].GetKey() != 0)
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
		if (this->diskPage[diskPageIndex].GetKey() == 0 || this->diskPage[diskPageIndex].GetDeleteFlag() == true)
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
				auto indexes = std::ofstream(indexArea->GetFilename(), std::ofstream::binary | std::ofstream::app);
				auto key = auxPage[0].GetKey();
				indexes.write(reinterpret_cast<char*>(&key), sizeof(uint32_t));

				indexes.close();
			}
			currentOccupation++;
		}
		else
		{
			this->AppendRawPage(primaryArea->GetFilename(), auxPage);

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
		if (auxPage[0].GetKey() > 0)
		{
			this->AppendRawPage(primaryArea->GetFilename(), auxPage);
		}
		else
		{
			this->UpdateAreasLength();
			this->UpdateRawPage(primaryArea->GetFilename(), auxPage,
			                    (indexArea->GetLength() - 1) / this->indexAreaRecordSize * blockingFactor);
		}
	}
	delete[] auxPage;
}


std::pair<uint32_t, AreaRecord> Dbms::FindAreaRecordInOverflow(const uint32_t key, uint32_t pointer)
{
	const auto auxPage = new AreaRecord[blockingFactor];
	uint32_t selectedIndex = 0, anchor = pointer;

	this->GetRawPage(overflowArea->GetFilename(), pointer - 1, auxPage);

	while (pointer != 0)
	{
		const uint32_t lastPointer = pointer;
		pointer = auxPage[selectedIndex].GetPointer();

		if (auxPage[selectedIndex].GetKey() == key)
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
			this->GetRawPage(overflowArea->GetFilename(), pointer - 1, auxPage);
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
		if (this->diskPage[i].GetKey() == key)
		{
			return this->diskPage[i];
		}

		// If record can only be linked to base pointer
		if (this->diskPage[i].GetKey() > key && i == 0)
		{
			return this->FindAreaRecordInOverflow(key, this->basePointer).second;
		}

		// If there's a link to overflow area in the middle of page
		if (i > 0 && this->diskPage[i].GetKey() > key && this->diskPage[i - 1].GetPointer() > 0)
		{
			return this->FindAreaRecordInOverflow(key, this->diskPage[i - 1].GetPointer()).second;
		}
	}

	// If there's a link to overflow on the very last record
	if (this->diskPage[blockingFactor - 1].GetPointer() > 0)
	{
		return this->FindAreaRecordInOverflow(key, this->diskPage[blockingFactor - 1].GetPointer()).second;
	}

	return AreaRecord(0, Record(0, 0), 0, false);
}

bool Dbms::UpdateAreaRecordInOverflow(const uint32_t key, const Record data, const uint32_t startPointer)
{
	auto result = this->FindAreaRecordInOverflow(key, startPointer);

	if (result.first > 0)
	{
		result.second.SetRecord(data);
		result.second.SetDeleteFlag(false);

		this->diskOperations++;
		auto file = std::ofstream(primaryArea->GetFilename(), std::ofstream::binary | std::ofstream::in);
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

uint32_t Dbms::BinarySearchPage(uint32_t key)
{
	const size_t indexPageSize = blockingFactor * fullAreaRecordSize / indexAreaRecordSize * indexAreaRecordSize;
	const size_t pages = this->indexArea->GetLength() / indexPageSize + 1;
	uint32_t left = 0, right = pages, pointer = 0;

	const auto indexPage = new uint32_t[indexPageSize];
	size_t indexPageLength = 0;

	// External binary search
	bool found = false;
	while (!found)
	{
		uint32_t last;

		this->diskOperations++;
		auto file = std::ifstream(this->indexArea->GetFilename(), std::ifstream::binary);

		pointer = (left + right) / 2;

		// First page record
		const uint32_t first = this->indexArea->GetRecord(file, pointer * indexPageSize);

		// Last page record
		bool fullPageWasRead = true;
		if ((pointer + 1) * indexPageSize - indexAreaRecordSize > this->indexArea->GetLength())
		{
			last = this->indexArea->GetRecord(file, this->indexArea->GetLength() - indexAreaRecordSize);
			fullPageWasRead = false;
		}
		else
		{
			last = this->indexArea->GetRecord(file, (pointer + 1) * indexPageSize - indexAreaRecordSize);
		}

		if (first <= key && last >= key || !fullPageWasRead || pointer == 0 && first > key)
		{
			const unsigned page = pointer * indexPageSize;
			file.seekg(page, std::basic_ifstream<char>::beg);
			do
			{
				file.read(reinterpret_cast<char*>(&indexPage[indexPageLength]), indexAreaRecordSize);
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

			right = right - left != 1 ? right : right - 1; // fix stuck on 0.5 issue
		}
	}
	delete[] indexPage;

	return pointer + 1; // returns position, not index
}

uint32_t Dbms::GetDiskPage(const uint32_t key)
{
	const uint32_t pageNo = this->BinarySearchPage(key);

	this->diskOperations++;
	auto file = std::ifstream(primaryArea->GetFilename(), std::ifstream::binary);

	const unsigned pageBeginningPosition = (pageNo - 1) * fullAreaRecordSize * blockingFactor;
	file.seekg(pageBeginningPosition, std::basic_ifstream<char>::beg);

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
	auto file = std::ofstream(primaryArea->GetFilename(), std::ofstream::binary | std::ofstream::in);

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
		result.second.SetDeleteFlag(true);

		this->diskOperations++;
		auto file = std::ofstream(primaryArea->GetFilename(), std::ofstream::binary | std::ofstream::in);
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
			auto output = std::ofstream(overflowArea->GetFilename(), std::ofstream::binary | std::ofstream::app);

			this->AppendAreaRecord(output, AreaRecord(key, record, 0, false));
			output.close();

			startPointer = overflowArea->GetLength() / this->fullAreaRecordSize + 1;
			this->UpdateAreasLength();
		}

		return;
	}

	const auto auxPage = new AreaRecord[blockingFactor];
	uint32_t selectedIndex = 0, anchor = startPointer, lastPointer = startPointer, pointer = startPointer;
	bool alreadyInserted = false;

	this->GetRawPage(overflowArea->GetFilename(), pointer - 1, auxPage);

	while (pointer != 0)
	{
		lastPointer = pointer;
		pointer = auxPage[selectedIndex].GetPointer();

		// If record already exist, just updateFileStructure it's data and turn off deleteFlag
		if (auxPage[selectedIndex].GetKey() == key)
		{
			alreadyInserted = true;

			this->diskOperations++;
			auto file = std::ofstream(overflowArea->GetFilename(), std::ofstream::binary | std::ofstream::in);

			this->SetAreaRecord(file, AreaRecord(key, record, pointer, false), lastPointer - 1);
			file.close();

			break;
		}

		// If there's greater record than the current, override current and set pointer to the new record location
		if (auxPage[selectedIndex].GetKey() > key)
		{
			alreadyInserted = true;

			auto file = std::ofstream(overflowArea->GetFilename(), std::ofstream::binary | std::ofstream::in);

			// Update record with the inserted one
			this->diskOperations++;
			this->SetAreaRecord(file,
			                    AreaRecord(key, record, overflowArea->GetLength() / this->fullAreaRecordSize + 1, false),
			                    lastPointer - 1);
			file.close();
			file = std::ofstream(overflowArea->GetFilename(), std::ofstream::binary | std::ofstream::app);

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
			this->GetRawPage(overflowArea->GetFilename(), pointer - 1, auxPage);
			anchor = pointer;
			selectedIndex = 0;
		}
	}

	if (!alreadyInserted)
	{
		auto file = std::ofstream(overflowArea->GetFilename(), std::ofstream::binary | std::ofstream::in);

		AreaRecord recordToUpdate = auxPage[selectedIndex];
		recordToUpdate.SetPointer(overflowArea->GetLength() / this->fullAreaRecordSize + 1);

		// Set pointer to the new record
		this->diskOperations++;
		this->SetAreaRecord(file, recordToUpdate, lastPointer - 1);

		file.close();
		file = std::ofstream(overflowArea->GetFilename(), std::ofstream::binary | std::ofstream::app);

		// Append new record
		this->diskOperations++;
		this->AppendAreaRecord(file, AreaRecord(key, record, 0, false));

		file.close();
	}
	delete[] auxPage;
	this->UpdateAreasLength();
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
		if (this->diskPage[i].GetKey() == 0)
		{
			this->diskPage[i] = AreaRecord(key, record, 0, false);
			isInserted = true;
		}

		// Update data if it's a duplicate
		else if (this->diskPage[i].GetKey() == key)
		{
			this->diskPage[i].SetRecord(record);
			this->diskPage[i].SetDeleteFlag(false);
			isInserted = true;
		}

		// Break if cannot fit in the record
		else if (this->diskPage[i].GetKey() > key)
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
			auto startPointer = this->diskPage[i - 1].GetPointer();
			this->InsertToOverflow(key, record, startPointer);
			this->diskPage[i - 1].SetPointer(startPointer);
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
		pointer = auxPage[overflowIndex].GetPointer();

		this->diskPage[index] = auxPage[overflowIndex];
		this->diskPage[index].SetPointer(0);
		this->diskPage[index].SetDeleteFlag(false);

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
			if (i >= blockingFactor || primaryArea->GetLength() == 0)
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
			this->diskPage[i].SetKey(auxPage[primaryIterator].GetKey());
			this->diskPage[i].SetRecord(auxPage[primaryIterator].GetRecord());
			this->diskPage[i].SetDeleteFlag(auxPage[primaryIterator].GetDeleteFlag());
			lastPointer = auxPage[primaryIterator].GetPointer();
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
	uint32_t currentPageOccupation = 0;

	const uint32_t maxPosition = primaryArea->GetLength() / fullAreaRecordSize;
	while (lastPosition < maxPosition || lastPointer > 0)
	{
		this->GetPageToReorganize(lastPosition, lastPointer);
		this->AppendPageWithAlphaCorrection(currentPageOccupation);
	}

	this->basePointer = 0;
	this->BackupBasePointer();
	this->UpdateAreasLength();
}


Dbms::Dbms(const uint32_t blockingFactor, const double alpha, const double maxOverflowOccupation,
	const std::string& primaryAreaFilename, const std::string& overflowAreaFilename, const std::string& indexAreaFilename) :
	diskPage(new AreaRecord[blockingFactor]), blockingFactor(blockingFactor),
	alpha(alpha), maxOverflowOccupation(maxOverflowOccupation)
{
	primaryArea = new PrimaryArea(primaryAreaFilename, fullAreaRecordSize);
	overflowArea = new OverflowArea(overflowAreaFilename, fullAreaRecordSize);
	indexArea = new IndexArea(indexAreaFilename, indexAreaRecordSize);

	this->RecreateAreas(false);

	// Init base pointer first time if necessary
	auto writeBasePointerFile = std::ofstream("basePointer.bin", std::ofstream::binary | std::ofstream::app);
	if (FileUtils::GetFileLength(writeBasePointerFile) == 0)
	{
		writeBasePointerFile.write(reinterpret_cast<char*>(&this->basePointer), sizeof(uint32_t));
	}
	writeBasePointerFile.close();

	auto readBasePointerFile = std::ifstream("basePointer.bin", std::ifstream::binary);
	readBasePointerFile.read(reinterpret_cast<char*>(&this->basePointer), sizeof(uint32_t));
	readBasePointerFile.close();

	this->UpdateAreasLength();
}


void Dbms::UpdateFileStructure(const bool forceUpdate)
{
	if (primaryArea->GetLength() == 0 ||
		overflowArea->GetLength() / static_cast<double>(primaryArea->GetLength()) >= maxOverflowOccupation ||
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

	if (primaryArea->GetLength() == 0)
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
		if (this->diskPage[i].GetKey() == 0)
		{
			std::cout << "Record doesn't exist!" << std::endl;
			return;
		}

		// Update data on the same key
		if (this->diskPage[i].GetKey() == key)
		{
			this->diskPage[i].SetRecord(record);
			this->diskPage[i].SetDeleteFlag(false);
			isInserted = true;
		}

		// Break if cannot fit in the record
		else if (this->diskPage[i].GetKey() > key)
		{
			break;
		}
	}

	if (!isInserted)
	{
		const auto recordInOverflow = this->FindAreaRecordInOverflow(
			key, (i == 0) ? this->basePointer : this->diskPage[i - 1].GetPointer());
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
	if (areaRecord.GetKey() > 0)
	{
		this->Insert(newKey, areaRecord.GetRecord());
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
		if (this->diskPage[i].GetKey() == 0)
		{
			std::cout << "Record doesn't exist!" << std::endl;
			return areaRecord;
		}

		// Select record to delete
		if (this->diskPage[i].GetKey() == key)
		{
			areaRecord = this->diskPage[i];
			this->diskPage[i].SetDeleteFlag(true);
			break;
		}

		if (this->diskPage[i].GetKey() > key)
		{
			if (i == 0)
			{
				areaRecord = this->SetToDeleteInOverflow(key, this->basePointer);
			}
			else
			{
				areaRecord = this->SetToDeleteInOverflow(key, this->diskPage[i].GetPointer());
			}

			return areaRecord;
		}
	}

	this->SetDiskPage(pageNo);
	return areaRecord;
}

void Dbms::Read(const uint32_t key)
{
	const auto areaRecord = this->FindAreaRecord(key);
	if (areaRecord.GetKey() != 0)
	{
		std::cout << "Data: (" << areaRecord.GetRecord().getInput().first << ", " << areaRecord.GetRecord().getInput().second << ")"
			<< std::endl;
		std::cout << "Pointer: " << areaRecord.GetPointer() << std::endl;
		std::cout << "DeleteFlag: " << areaRecord.GetDeleteFlag() << std::endl;
	}
	else
	{
		std::cout << "Requested record with key " << key << " doesn't exist!" << std::endl;
	}
}


void Dbms::PrintIndex() const
{
	auto file = std::ifstream(indexArea->GetFilename(), std::ifstream::binary);

	std::cout << " === Index === " << std::endl;
	std::cout << "key" << std::endl;

	const size_t length = FileUtils::GetFileLength(file) / sizeof(uint32_t);

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
	auto file = std::ifstream(primaryArea->GetFilename(), std::ifstream::binary);

	std::cout << " === Primary === " << std::endl;
	std::cout << "key\tdata\t\tpointer\tdelete" << std::endl;

	const size_t length = FileUtils::GetFileLength(file) / fullAreaRecordSize;
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
		if (this->diskPage[i].GetKey() == 0)
		{
			std::cout << "<empty record>" << std::endl;
		}
		else
		{
			std::cout <<
				this->diskPage[i].GetKey() << "\t(" <<
				this->diskPage[i].GetRecord().getInput().first << ", " <<
				this->diskPage[i].GetRecord().getInput().second << ")\t\t" <<
				this->diskPage[i].GetPointer() << "\t" <<
				this->diskPage[i].GetDeleteFlag() <<
				std::endl;
		}
	}
	std::cout << std::endl;
}

void Dbms::PrintOverflow() const
{
	auto file = std::ifstream(overflowArea->GetFilename(), std::ifstream::binary);

	std::cout << " === Overflow === " << std::endl;
	std::cout << "key\tdata\t\tpointer\tdelete" << std::endl;

	const size_t length = FileUtils::GetFileLength(file) / fullAreaRecordSize;
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
