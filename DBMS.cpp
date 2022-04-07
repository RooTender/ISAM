#include "Dbms.h"

void Dbms::InitializeBasePointer()
{
	this->basePointerFilename = 
		FileUtils::GetFilenameWithoutExtenstion(primaryArea->GetFilename()) +
		FileUtils::GetFilenameWithoutExtenstion(overflowArea->GetFilename()) +
		FileUtils::GetFilenameWithoutExtenstion(indexArea->GetFilename()) +
		"_basePointer.bin";
}

void Dbms::BackupBasePointer()
{
	this->diskOperations++;
	auto file = std::ofstream(this->basePointerFilename, std::ofstream::binary | std::ofstream::trunc);
	file.write(reinterpret_cast<char*>(&this->basePointer), sizeof(unsigned));
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


unsigned Dbms::GetIndexRecord(std::ifstream& file, const unsigned index) const
{
	unsigned record = 0;

	file.seekg(index, std::ifstream::beg);
	file.read(reinterpret_cast<char*>(&record), indexAreaRecordSize);

	return record;
}


AreaRecord Dbms::ReadAreaRecord(std::ifstream& file)
{
	unsigned key = 0, pointer = 0;
	double radius = 0, height = 0;
	bool deleteFlag = false;

	file.read(reinterpret_cast<char*>(&key), sizeof(unsigned));
	file.read(reinterpret_cast<char*>(&radius), sizeof(double));
	file.read(reinterpret_cast<char*>(&height), sizeof(double));
	file.read(reinterpret_cast<char*>(&pointer), sizeof(unsigned));
	file.read(reinterpret_cast<char*>(&deleteFlag), sizeof(bool));

	return AreaRecord(key, Record(radius, height), pointer, deleteFlag);
}

AreaRecord Dbms::ReadAreaRecord(std::ifstream& file, const unsigned index) const
{
	const std::streampos currentPosition = file.tellg();
	const auto selectedPage = index * fullAreaRecordSize;

	file.seekg(selectedPage, std::ifstream::beg);
	AreaRecord areaRecord = this->ReadAreaRecord(file);

	file.seekg(currentPosition);

	return areaRecord;
}

void Dbms::WriteAreaRecord(std::ofstream& file, const AreaRecord& record)
{
	unsigned key = record.GetKey(), pointer = record.GetPointer();
	double radius = record.GetRecord().GetInput().first;
	double height = record.GetRecord().GetInput().second;
	bool deleteFlag = record.GetDeleteFlag();

	file.write(reinterpret_cast<char*>(&key), sizeof(unsigned));
	file.write(reinterpret_cast<char*>(&radius), sizeof(double));
	file.write(reinterpret_cast<char*>(&height), sizeof(double));
	file.write(reinterpret_cast<char*>(&pointer), sizeof(unsigned));
	file.write(reinterpret_cast<char*>(&deleteFlag), sizeof(bool));
}

void Dbms::WriteAreaRecordOnPage(std::ofstream& file, const AreaRecord record, const unsigned index) const
{
	const auto selectedPage = index * static_cast<unsigned>(fullAreaRecordSize);
	file.seekp(selectedPage, std::ofstream::beg);

	this->WriteAreaRecord(file, record);
}

bool Dbms::IsNextRecordOnCurrentPage(const unsigned& pageAnchor, const unsigned& pointerToNextRecord) const
{
	return pointerToNextRecord >= pageAnchor && pointerToNextRecord <= pageAnchor + blockingFactor - 1 ? true : false;
}


void Dbms::GetRawPage(const std::string& filename, const unsigned& index, AreaRecord* dest)
{
	this->diskOperations++;
	auto file = std::ifstream(filename, std::ifstream::binary);
	const auto maxPosition = FileUtils::GetFileLength(file) / fullAreaRecordSize;

	for (size_t i = 0; i < blockingFactor && index + i < maxPosition; ++i)
	{
		dest[i] = this->ReadAreaRecord(file, index + static_cast<unsigned>(i));
	}
	file.close();
}

void Dbms::AppendRawPage(const std::string& filename, const AreaRecord* src)
{
	this->diskOperations++;
	auto file = std::ofstream(filename, std::ofstream::binary | std::ofstream::app);

	for (size_t i = 0; i < blockingFactor; ++i)
	{
		this->WriteAreaRecord(file, src[i]);
	}
	file.close();
}

void Dbms::UpdateRawPage(const std::string& string, const AreaRecord* auxPage, const unsigned anchor)
{
	this->diskOperations++;
	auto file = std::ofstream(string, std::ofstream::binary | std::ofstream::in);

	for (size_t i = 0; i < blockingFactor; ++i)
	{
		if (auxPage[i].GetKey() != 0)
		{
			this->WriteAreaRecordOnPage(file, auxPage[i], anchor + static_cast<unsigned>(i));
		}
	}
}

void Dbms::AppendPageWithAlphaCorrection(unsigned& currentOccupation)
{
	const auto auxPage = new AreaRecord[blockingFactor];
	const auto limit = static_cast<unsigned>(std::floor(alpha * blockingFactor));
	unsigned diskPageIndex = 0;
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
				indexes.write(reinterpret_cast<char*>(&key), sizeof(unsigned));

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
			                    (static_cast<unsigned>(indexArea->GetLength()) - 1) / this->indexAreaRecordSize * blockingFactor);
		}
	}
	delete[] auxPage;
}


std::pair<unsigned, AreaRecord> Dbms::FindAreaRecordInOverflow(const unsigned key, unsigned pointer)
{
	const auto auxPage = new AreaRecord[blockingFactor];
	unsigned selectedIndex = 0, anchor = pointer;

	this->GetRawPage(overflowArea->GetFilename(), pointer - 1, auxPage);

	while (pointer != 0)
	{
		const unsigned lastPointer = pointer;
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

AreaRecord Dbms::FindAreaRecord(const unsigned key)
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

bool Dbms::UpdateAreaRecordInOverflow(const unsigned key, const Record data, const unsigned startPointer)
{
	auto result = this->FindAreaRecordInOverflow(key, startPointer);

	if (result.first > 0)
	{
		result.second.SetRecord(data);
		result.second.SetDeleteFlag(false);

		this->diskOperations++;
		auto file = std::ofstream(primaryArea->GetFilename(), std::ofstream::binary | std::ofstream::in);
		this->WriteAreaRecordOnPage(file, result.second, result.first - 1);
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

unsigned Dbms::BinarySearchPage(const unsigned key)
{
	const unsigned indexPageSize = blockingFactor * fullAreaRecordSize / indexAreaRecordSize * indexAreaRecordSize;
	const unsigned pages = static_cast<unsigned>(this->indexArea->GetLength()) / indexPageSize + 1;
	unsigned left = 0, right = pages, pointer = 0;

	const auto indexPage = new unsigned[indexPageSize];
	unsigned currentIndexPageLength = 0;

	// External binary search
	bool found = false;
	while (!found)
	{
		unsigned last;

		this->diskOperations++;
		auto file = std::ifstream(this->indexArea->GetFilename(), std::ifstream::binary);

		pointer = (left + right) / 2;

		// First page record
		const unsigned first = this->indexArea->GetRecord(file, pointer * indexPageSize);

		// Last page record
		bool fullPageWasRead = true;
		if ((static_cast<unsigned long long>(pointer) + 1) * indexPageSize - indexAreaRecordSize > this->indexArea->GetLength())
		{
			last = this->indexArea->GetRecord(file, static_cast<unsigned>(this->indexArea->GetLength()) - indexAreaRecordSize);
			fullPageWasRead = false;
		}
		else
		{
			last = this->indexArea->GetRecord(file, (pointer + 1) * indexPageSize - indexAreaRecordSize);
		}

		if (first <= key && last >= key || !fullPageWasRead || pointer == 0 && first > key)
		{
			const auto page = pointer * static_cast<unsigned>(indexPageSize);
			file.seekg(page, std::basic_ifstream<char>::beg);
			do
			{
				file.read(reinterpret_cast<char*>(&indexPage[currentIndexPageLength]), indexAreaRecordSize);
			} while (currentIndexPageLength < indexPageSize && indexPage[currentIndexPageLength++] != last);

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
	right = currentIndexPageLength;

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

unsigned Dbms::GetDiskPage(const unsigned key)
{
	const unsigned pageNo = this->BinarySearchPage(key);

	this->diskOperations++;
	auto file = std::ifstream(primaryArea->GetFilename(), std::ifstream::binary);

	const unsigned pageBeginningPosition = (pageNo - 1) * static_cast<unsigned>(fullAreaRecordSize * blockingFactor);
	file.seekg(pageBeginningPosition, std::basic_ifstream<char>::beg);

	for (size_t i = 0; i < blockingFactor; ++i)
	{
		this->diskPage[i] = this->ReadAreaRecord(file);
	}
	file.close();

	return pageNo;
}

void Dbms::SetDiskPage(const unsigned pageNo)
{
	this->diskOperations++;
	auto file = std::ofstream(primaryArea->GetFilename(), std::ofstream::binary | std::ofstream::in);

	for (size_t i = 0; i < blockingFactor; ++i)
	{
		this->WriteAreaRecordOnPage(file, this->diskPage[i], (pageNo - 1) * blockingFactor + static_cast<unsigned>(i));
	}
	file.close();
}


AreaRecord Dbms::SetToDeleteInOverflow(const unsigned key, const unsigned pointer)
{
	auto result = this->FindAreaRecordInOverflow(key, pointer);

	if (result.first > 0)
	{
		result.second.SetDeleteFlag(true);

		this->diskOperations++;
		auto file = std::ofstream(primaryArea->GetFilename(), std::ofstream::binary | std::ofstream::in);
		this->WriteAreaRecordOnPage(file, result.second, result.first - 1);
		file.close();
	}
	else
	{
		std::cout << "Record doesn't exist" << std::endl;
	}

	return result.second;
}


void Dbms::InsertToOverflow(unsigned key, Record record, unsigned& startPointer)
{
	// If pointer wasn't assigned, just append new record to overflow
	if (startPointer == 0)
	{
		// If record wasn't already existed
		if (!this->UpdateAreaRecordInOverflow(key, record, startPointer))
		{
			this->diskOperations++;
			auto output = std::ofstream(overflowArea->GetFilename(), std::ofstream::binary | std::ofstream::app);

			this->WriteAreaRecord(output, AreaRecord(key, record, 0, false));
			output.close();

			startPointer = static_cast<unsigned>(overflowArea->GetLength()) / this->fullAreaRecordSize + 1;
			this->UpdateAreasLength();
		}

		return;
	}

	const auto auxPage = new AreaRecord[blockingFactor];
	unsigned selectedIndex = 0, anchor = startPointer, lastPointer = startPointer, pointer = startPointer;
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

			this->WriteAreaRecordOnPage(file, AreaRecord(key, record, pointer, false), lastPointer - 1);
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
			this->WriteAreaRecordOnPage(file,
			                    AreaRecord(key, record, static_cast<unsigned>(overflowArea->GetLength()) / this->fullAreaRecordSize + 1, false),
			                    lastPointer - 1);
			file.close();
			file = std::ofstream(overflowArea->GetFilename(), std::ofstream::binary | std::ofstream::app);

			// Append old record
			this->diskOperations++;
			AreaRecord areaRecord = auxPage[selectedIndex];
			this->WriteAreaRecord(file, areaRecord);

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
		recordToUpdate.SetPointer(static_cast<unsigned>(overflowArea->GetLength()) / this->fullAreaRecordSize + 1);

		// Set pointer to the new record
		this->diskOperations++;
		this->WriteAreaRecordOnPage(file, recordToUpdate, lastPointer - 1);

		file.close();
		file = std::ofstream(overflowArea->GetFilename(), std::ofstream::binary | std::ofstream::app);

		// Append new record
		this->diskOperations++;
		this->WriteAreaRecord(file, AreaRecord(key, record, 0, false));

		file.close();
	}
	delete[] auxPage;
	this->UpdateAreasLength();
}

void Dbms::InsertToBasePointer(const unsigned key, const Record record)
{
	this->InsertToOverflow(key, record, this->basePointer);
	this->BackupBasePointer();
}

void Dbms::InsertToPrimary(const unsigned key, const Record record)
{
	const unsigned pageNo = this->GetDiskPage(key);

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


void Dbms::FillRecordsFromOverflow(unsigned& pointer, unsigned& index)
{
	const auto auxPage = new AreaRecord[blockingFactor];
	unsigned overflowIndex = 0, anchor = pointer;

	this->GetRawPage(this->overflowArea->GetBackupFilename(), pointer - 1, auxPage);

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

			this->GetRawPage(this->overflowArea->GetBackupFilename(), pointer - 1, auxPage);
			anchor = pointer;
			overflowIndex = 0;
		}

		index++;
	}

	delete[] auxPage;
}

void Dbms::GetPageToReorganize(unsigned& lastPosition, unsigned& lastPointer)
{
	this->ClearDiskPage();

	const auto auxPage = new AreaRecord[blockingFactor];
	bool primaryLoaded = false;

	unsigned primaryIterator = 0;
	for (unsigned i = 0; i < blockingFactor; ++i)
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
			this->GetRawPage(this->primaryArea->GetBackupFilename(), lastPosition - 1, auxPage);
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

	unsigned lastPosition = 1;
	unsigned lastPointer = this->basePointer;
	unsigned currentPageOccupation = 0;

	const auto maxPosition = static_cast<unsigned>(primaryArea->GetLength() / fullAreaRecordSize);
	while (lastPosition < maxPosition || lastPointer > 0)
	{
		this->GetPageToReorganize(lastPosition, lastPointer);
		this->AppendPageWithAlphaCorrection(currentPageOccupation);
	}

	this->basePointer = 0;
	this->BackupBasePointer();
	this->UpdateAreasLength();
}


Dbms::Dbms(const unsigned blockingFactor, const double alpha, const double maxOverflowOccupation,
	const std::string& primaryAreaFilename, const std::string& overflowAreaFilename, const std::string& indexAreaFilename) :
	diskPage(new AreaRecord[blockingFactor]), alpha(alpha),
	maxOverflowOccupation(maxOverflowOccupation), blockingFactor(blockingFactor)
{
	primaryArea = new PrimaryArea(primaryAreaFilename, fullAreaRecordSize);
	overflowArea = new OverflowArea(overflowAreaFilename, fullAreaRecordSize);
	indexArea = new IndexArea(indexAreaFilename, indexAreaRecordSize);

	this->RecreateAreas(false);

	// Init base pointer first time if necessary
	auto writeBasePointerFile = std::ofstream(this->basePointerFilename, std::ofstream::binary | std::ofstream::app);
	if (FileUtils::GetFileLength(writeBasePointerFile) == 0)
	{
		writeBasePointerFile.write(reinterpret_cast<char*>(&this->basePointer), sizeof(unsigned));
	}
	writeBasePointerFile.close();

	auto readBasePointerFile = std::ifstream(this->basePointerFilename, std::ifstream::binary);
	readBasePointerFile.read(reinterpret_cast<char*>(&this->basePointer), sizeof(unsigned));
	readBasePointerFile.close();

	this->UpdateAreasLength();
}


void Dbms::UpdateFileStructure(const bool forceUpdate)
{
	const double overflowToPrimaryAreaRatio = 
		static_cast<double>(overflowArea->GetLength()) / static_cast<double>(primaryArea->GetLength());

	if (primaryArea->GetLength() == 0 || overflowToPrimaryAreaRatio >= maxOverflowOccupation || forceUpdate)
	{
		this->Reorganize();
	}
}

void Dbms::Insert(const unsigned key, const Record record)
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

void Dbms::UpdateRecord(const unsigned key, const Record record)
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

void Dbms::UpdateKey(const unsigned oldKey, const unsigned newKey)
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

AreaRecord Dbms::Remove(const unsigned key)
{
	const unsigned pageNo = this->GetDiskPage(key);
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

void Dbms::Read(const unsigned key)
{
	const auto areaRecord = this->FindAreaRecord(key);
	if (areaRecord.GetKey() != 0)
	{
		std::cout << "Data: (" << areaRecord.GetRecord().GetInput().first << ", " << areaRecord.GetRecord().GetInput().second << ")"
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

	const size_t length = FileUtils::GetFileLength(file) / sizeof(unsigned);

	for (size_t i = 0; i < length; ++i)
	{
		unsigned key = 0;
		file.read(reinterpret_cast<char*>(&key), sizeof(unsigned));

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
		unsigned key = 0, pointer = 0;
		double radius = 0, height = 0;
		bool deleteFlag = false;

		file.read(reinterpret_cast<char*>(&key), sizeof(unsigned)); // key
		file.read(reinterpret_cast<char*>(&radius), sizeof(double)); // data: radius
		file.read(reinterpret_cast<char*>(&height), sizeof(double)); // data: height
		file.read(reinterpret_cast<char*>(&pointer), sizeof(unsigned)); // pointer
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
				this->diskPage[i].GetRecord().GetInput().first << ", " <<
				this->diskPage[i].GetRecord().GetInput().second << ")\t\t" <<
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
		AreaRecord areaRecord = this->ReadAreaRecord(file);

		std::cout << areaRecord.GetKey() << 
			"\t(" << areaRecord.GetRecord().GetInput().first << ", " << areaRecord.GetRecord().GetInput().second << ")\t" <<
			"\t" << areaRecord.GetPointer() << 
			"\t" << areaRecord.GetDeleteFlag() << std::endl;
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
