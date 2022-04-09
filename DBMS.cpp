#include "Dbms.h"

void Dbms::InitializeBasePointer()
{
	this->basePointerFilename = "./" +
		FileUtils::GetFilenameWithoutPathAndExtenstion(primaryArea->GetFilename()) +
		FileUtils::GetFilenameWithoutPathAndExtenstion(overflowArea->GetFilename()) +
		FileUtils::GetFilenameWithoutPathAndExtenstion(indexArea->GetFilename()) +
		"_basePointer.bin";

	auto writeToFile = std::ofstream(this->basePointerFilename, std::ofstream::binary | std::ofstream::app);
	if (FileUtils::GetFileLength(writeToFile) == 0)
	{
		writeToFile.write(reinterpret_cast<char*>(&this->basePointer), sizeof(unsigned));
	}
	writeToFile.close();

	auto readFile = std::ifstream(this->basePointerFilename, std::ifstream::binary);
	readFile.read(reinterpret_cast<char*>(&this->basePointer), sizeof(unsigned));
	readFile.close();
}

void Dbms::UpdateBasePointer()
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

void Dbms::OverrideAreaRecordOnPage(std::ofstream& file, const AreaRecord record, const unsigned index) const
{
	const auto selectedPage = index * areaRecordSize;
	file.seekp(selectedPage, std::ofstream::beg);

	DbmsUtils::WriteAreaRecord(file, record);
}

bool Dbms::IsNextRecordOnCurrentPage(const unsigned& pageAnchor, const unsigned& pointerToNextRecord) const
{
	return pointerToNextRecord >= pageAnchor && pointerToNextRecord <= pageAnchor + blockingFactor - 1 ? true : false;
}


void Dbms::FillWithAreaRecords(const std::string& filename, const unsigned& index, AreaRecord* dest)
{
	this->diskOperations++;
	auto file = std::ifstream(filename, std::ifstream::binary);
	const auto maxPosition = FileUtils::GetFileLength(file) / areaRecordSize;

	for (size_t i = 0; i < blockingFactor && index + i < maxPosition; ++i)
	{
		dest[i] = dbmsUtils->ReadAreaRecord(file, index + static_cast<unsigned>(i));
	}
	file.close();
}

void Dbms::AppendRawPage(const std::string& filename, const AreaRecord* src)
{
	this->diskOperations++;
	auto file = std::ofstream(filename, std::ofstream::binary | std::ofstream::app);

	for (size_t i = 0; i < blockingFactor; ++i)
	{
		DbmsUtils::WriteAreaRecord(file, src[i]);
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
			this->OverrideAreaRecordOnPage(file, auxPage[i], anchor + static_cast<unsigned>(i));
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
		if (this->diskPage->Get(diskPageIndex).GetKey() == 0 || this->diskPage->Get(diskPageIndex).GetDeleteFlag() == true)
		{
			diskPageIndex++;
			continue;
		}
		pageIsEmpty = false;

		if (currentOccupation < limit)
		{
			auxPage[currentOccupation] = this->diskPage->Get(diskPageIndex);
			diskPageIndex++;

			// If new page => add key to indexes
			if (currentOccupation == 0)
			{
				this->diskOperations++;
				auto file = std::ofstream(this->indexArea->GetFilename(), std::ofstream::binary | std::ofstream::in);
				DbmsUtils::WriteIndexRecord(file, auxPage[0].GetKey());

				file.close();
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

	currentOccupation = currentOccupation < limit ? currentOccupation : 0;
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
			                    (static_cast<unsigned>(indexArea->GetLength()) - 1) / this->indexRecordSize * blockingFactor);
		}
	}
	delete[] auxPage;
}


std::pair<unsigned, AreaRecord> Dbms::FindAreaRecordInOverflow(const unsigned key, unsigned pointer)
{
	const auto page = new AreaRecord[blockingFactor];
	unsigned selectedIndex = 0, anchor = pointer;

	this->FillWithAreaRecords(overflowArea->GetFilename(), pointer - 1, page);

	while (pointer != 0)
	{
		const unsigned lastPointer = pointer;
		pointer = page[selectedIndex].GetPointer();

		if (page[selectedIndex].GetKey() == key)
		{
			AreaRecord areaRecord = page[selectedIndex];
			delete[] page;

			return {lastPointer, areaRecord};
		}

		if (this->IsNextRecordOnCurrentPage(anchor, pointer))
		{
			selectedIndex = pointer - anchor;
		}
		else
		{
			this->FillWithAreaRecords(overflowArea->GetFilename(), pointer - 1, page);
			anchor = pointer;
			selectedIndex = 0;
		}
	}

	delete[] page;
	return {0, AreaRecord(0, Record(0, 0), 0, false)};
}

AreaRecord Dbms::FindAreaRecord(const unsigned key)
{
	this->LoadPrimaryToDiskPage(key);

	for (size_t i = 0; i < blockingFactor; ++i)
	{
		// Key was found
		if (this->diskPage->Get(i).GetKey() == key)
		{
			return this->diskPage->Get(i);
		}

		// If record can only be linked to base pointer
		if (this->diskPage->Get(i).GetKey() > key && i == 0)
		{
			return this->FindAreaRecordInOverflow(key, this->basePointer).second;
		}

		// If there's a link to overflow area in the middle of page
		if (i > 0 && this->diskPage->Get(i).GetKey() > key && this->diskPage->Get(i - 1).GetPointer() > 0)
		{
			return this->FindAreaRecordInOverflow(key, this->diskPage->Get(i - 1).GetPointer()).second;
		}
	}

	// If there's a link to overflow on the very last record
	if (this->diskPage->Get(blockingFactor - 1).GetPointer() > 0)
	{
		return this->FindAreaRecordInOverflow(key, this->diskPage->Get(blockingFactor - 1).GetPointer()).second;
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
		this->OverrideAreaRecordOnPage(file, result.second, result.first - 1);
		file.close();

		return true;
	}
	return false;
}


unsigned Dbms::DeterminePageNumber(const unsigned key)
{
	const unsigned indexPageSize = blockingFactor * areaRecordSize / indexRecordSize * indexRecordSize;
	const unsigned pages = static_cast<unsigned>(this->indexArea->GetLength()) / indexPageSize + 1;

	unsigned left = 0;
	unsigned right = pages;
	unsigned pointer = 0;

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
		const unsigned first = dbmsUtils->ReadIndexRecord(file, pointer * indexPageSize);

		// Last page record
		bool fullPageWasRead = true;
		if ((static_cast<unsigned long long>(pointer) + 1) * indexPageSize - indexRecordSize > this->indexArea->GetLength())
		{
			last = dbmsUtils->ReadIndexRecord(file, static_cast<unsigned>(this->indexArea->GetLength()) - indexRecordSize);
			fullPageWasRead = false;
		}
		else
		{
			last = dbmsUtils->ReadIndexRecord(file, (pointer + 1) * indexPageSize - indexRecordSize);
		}

		if (first <= key && last >= key || !fullPageWasRead || pointer == 0 && first > key)
		{
			const auto page = pointer * indexPageSize;
			file.seekg(page, std::basic_ifstream<char>::beg);
			do
			{
				file.read(reinterpret_cast<char*>(&indexPage[currentIndexPageLength]), indexRecordSize);
			}
			while (currentIndexPageLength < indexPageSize && indexPage[currentIndexPageLength++] != last);

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

void Dbms::LoadPrimaryToDiskPage(const unsigned pageNumber)
{
	this->diskOperations++;
	auto file = std::ifstream(primaryArea->GetFilename(), std::ifstream::binary);

	const unsigned pageBeginningPosition = (pageNumber - 1) * areaRecordSize * blockingFactor;
	file.seekg(pageBeginningPosition, std::basic_ifstream<char>::beg);

	for (size_t i = 0; i < blockingFactor; ++i)
	{
		this->diskPage->Set(i, dbmsUtils->ReadAreaRecord(file, pageBeginningPosition + static_cast<unsigned>(i)));
	}
	file.close();
}

void Dbms::WriteDiskPageToPrimary(const unsigned pageNumber)
{
	this->diskOperations++;
	auto file = std::ofstream(primaryArea->GetFilename(), std::ofstream::binary | std::ofstream::in);

	for (size_t i = 0; i < blockingFactor; ++i)
	{
		this->OverrideAreaRecordOnPage(file, this->diskPage->Get(i), (pageNumber - 1) * blockingFactor + static_cast<unsigned>(i));
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
		this->OverrideAreaRecordOnPage(file, result.second, result.first - 1);
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
		// If the record wasn't already existed
		if (!this->UpdateAreaRecordInOverflow(key, record, startPointer))
		{
			this->diskOperations++;
			auto output = std::ofstream(overflowArea->GetFilename(), std::ofstream::binary | std::ofstream::app);

			DbmsUtils::WriteAreaRecord(output, AreaRecord(key, record, 0, false));
			output.close();

			startPointer = static_cast<unsigned>(overflowArea->GetLength()) / this->areaRecordSize + 1;
			this->UpdateAreasLength();
		}

		return;
	}

	const auto auxPage = new AreaRecord[blockingFactor];

	unsigned selectedIndex = 0;
	unsigned anchor = startPointer;
	unsigned lastPointer = startPointer;
	unsigned pointer = startPointer;

	bool alreadyInserted = false;

	this->FillWithAreaRecords(overflowArea->GetFilename(), pointer - 1, auxPage);

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

			this->OverrideAreaRecordOnPage(file, AreaRecord(key, record, pointer, false), lastPointer - 1);
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
			this->OverrideAreaRecordOnPage(file,
			                    AreaRecord(key, record, static_cast<unsigned>(overflowArea->GetLength()) / this->areaRecordSize + 1, false),
			                    lastPointer - 1);
			file.close();

			file = std::ofstream(overflowArea->GetFilename(), std::ofstream::binary | std::ofstream::app);

			// Append old record
			this->diskOperations++;
			AreaRecord areaRecord = auxPage[selectedIndex];
			DbmsUtils::WriteAreaRecord(file, areaRecord);

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
			this->FillWithAreaRecords(overflowArea->GetFilename(), pointer - 1, auxPage);
			anchor = pointer;
			selectedIndex = 0;
		}
	}

	if (!alreadyInserted)
	{
		auto file = std::ofstream(overflowArea->GetFilename(), std::ofstream::binary | std::ofstream::in);

		AreaRecord recordToUpdate = auxPage[selectedIndex];
		recordToUpdate.SetPointer(static_cast<unsigned>(overflowArea->GetLength()) / this->areaRecordSize + 1);

		// Set pointer to the new record
		this->diskOperations++;
		this->OverrideAreaRecordOnPage(file, recordToUpdate, lastPointer - 1);

		file.close();
		file = std::ofstream(overflowArea->GetFilename(), std::ofstream::binary | std::ofstream::app);

		// Append new record
		this->diskOperations++;
		DbmsUtils::WriteAreaRecord(file, AreaRecord(key, record, 0, false));

		file.close();
	}
	delete[] auxPage;
	this->UpdateAreasLength();
}

void Dbms::InsertToBasePointer(const unsigned key, const Record record)
{
	this->InsertToOverflow(key, record, this->basePointer);
	this->UpdateBasePointer();
}

void Dbms::InsertToPrimary(const unsigned key, const Record record)
{
	const unsigned page = this->DeterminePageNumber(key);
	this->LoadPrimaryToDiskPage(page);

	bool isInserted = false;
	size_t i;

	for (i = 0; i < blockingFactor && !isInserted; ++i)
	{
		// Insert if there's space
		if (this->diskPage->Get(i).GetKey() == 0)
		{
			this->diskPage->Set(i, AreaRecord(key, record, 0, false));
			isInserted = true;
		}

		// Update data if it's a duplicate
		else if (this->diskPage->Get(i).GetKey() == key)
		{
			AreaRecord areaRecord = this->diskPage->Get(i);
			areaRecord.SetRecord(record);
			areaRecord.SetDeleteFlag(false);

			this->diskPage->Set(i, areaRecord);
			isInserted = true;
		}

		// Break if cannot fit in the record
		else if (this->diskPage->Get(i).GetKey() > key)
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
			auto startPointer = this->diskPage->Get(i - 1).GetPointer();
			this->InsertToOverflow(key, record, startPointer);

			AreaRecord areaRecord = this->diskPage->Get(i - 1);
			areaRecord.SetPointer(startPointer);

			this->diskPage->Set(i - 1, areaRecord);
		}
	}

	this->WriteDiskPageToPrimary(page);
}


void Dbms::FillRecordsFromOverflow(unsigned& pointer, unsigned& index)
{
	const auto auxPage = new AreaRecord[blockingFactor];
	unsigned overflowIndex = 0, anchor = pointer;

	this->FillWithAreaRecords(this->overflowArea->GetBackupFilename(), pointer - 1, auxPage);

	while (index < blockingFactor && pointer != 0)
	{
		pointer = auxPage[overflowIndex].GetPointer();

		AreaRecord areaRecord = auxPage[overflowIndex];
		areaRecord.SetPointer(0);
		areaRecord.SetDeleteFlag(false);

		this->diskPage->Set(index, areaRecord);

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

			this->FillWithAreaRecords(this->overflowArea->GetBackupFilename(), pointer - 1, auxPage);
			anchor = pointer;
			overflowIndex = 0;
		}

		index++;
	}

	delete[] auxPage;
}

void Dbms::GetPageToReorganize(unsigned& lastPosition, unsigned& lastPointer)
{
	this->diskPage->Clear();

	const auto auxPage = new AreaRecord[blockingFactor];
	bool primaryLoaded = false;

	unsigned primaryIterator = 0;
	for (size_t i = 0; i < blockingFactor; ++i)
	{
		// The next record is in overflow
		if (lastPointer > 0)
		{
			auto stopIndex = static_cast<unsigned>(i);
			this->FillRecordsFromOverflow(lastPointer, stopIndex);

			if (stopIndex >= blockingFactor || primaryArea->GetLength() == 0)
			{
				break;
			}
			i = static_cast<size_t>(stopIndex);
		}

		// Load primary page
		if (!primaryLoaded)
		{
			this->FillWithAreaRecords(this->primaryArea->GetBackupFilename(), lastPosition - 1, auxPage);
			primaryLoaded = true;
		}

		if (primaryIterator < blockingFactor)
		{
			AreaRecord areaRecord = auxPage[primaryIterator];
			areaRecord.SetPointer(this->diskPage->Get(i).GetPointer());

			this->diskPage->Set(i, areaRecord);
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

	const auto maxPosition = static_cast<unsigned>(primaryArea->GetLength() / areaRecordSize);
	while (lastPosition < maxPosition || lastPointer > 0)
	{
		this->GetPageToReorganize(lastPosition, lastPointer);
		this->AppendPageWithAlphaCorrection(currentPageOccupation);
	}

	this->basePointer = 0;
	this->UpdateBasePointer();
	this->UpdateAreasLength();
}



Dbms::Dbms(const unsigned blockingFactor, const double alpha, const double maxOverflowOccupation,
           const std::string& primaryAreaFilename, const std::string& overflowAreaFilename, const std::string& indexAreaFilename) :
	diskPage(new DiskPage(blockingFactor)), alpha(alpha),
	maxOverflowOccupation(maxOverflowOccupation), blockingFactor(blockingFactor)
{
	primaryArea = new PrimaryArea(primaryAreaFilename, areaRecordSize);
	overflowArea = new OverflowArea(overflowAreaFilename, areaRecordSize);
	indexArea = new IndexArea(indexAreaFilename, indexRecordSize);

	dbmsUtils = new DbmsUtils(indexArea, primaryArea, overflowArea, areaRecordSize, indexRecordSize);

	this->InitializeBasePointer();
	this->RecreateAreas(false);
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
	const unsigned page = this->DeterminePageNumber(key);
	this->LoadPrimaryToDiskPage(page);

	bool isInserted = false;
	size_t i;

	for (i = 0; i < blockingFactor && !isInserted; ++i)
	{
		// Insert if there's space
		if (this->diskPage->Get(i).GetKey() == 0)
		{
			std::cout << "Record doesn't exist!" << std::endl;
			return;
		}

		// Update data on the same key
		if (this->diskPage->Get(i).GetKey() == key)
		{
			AreaRecord areaRecord = this->diskPage->Get(i);
			areaRecord.SetRecord(record);
			areaRecord.SetDeleteFlag(false);

			this->diskPage->Set(i, areaRecord);
			isInserted = true;
		}

		// Break if cannot fit in the record
		else if (this->diskPage->Get(i).GetKey() > key)
		{
			break;
		}
	}

	if (!isInserted)
	{
		const auto recordInOverflow = this->FindAreaRecordInOverflow(key, i == 0 ? this->basePointer : this->diskPage->Get(i - 1).GetPointer());
		if (recordInOverflow.first != 0)
		{
			this->UpdateAreaRecordInOverflow(key, record, recordInOverflow.first);
		}

		std::cout << "Record doesn't exist!" << std::endl;
	}
	else
	{
		// We don't need to updateRecord the pointer in primary, no need to set the page
		this->WriteDiskPageToPrimary(page);
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
	const unsigned page = this->DeterminePageNumber(key);
	this->LoadPrimaryToDiskPage(page);

	auto areaRecord = AreaRecord();

	for (size_t i = 0; i < blockingFactor; ++i)
	{
		// If there's space => record simply doesn't exist
		if (this->diskPage->Get(i).GetKey() == 0)
		{
			std::cout << "Record doesn't exist!" << std::endl;
			return areaRecord;
		}

		// Select record to delete
		if (this->diskPage->Get(i).GetKey() == key)
		{
			areaRecord = this->diskPage->Get(i);

			AreaRecord diskPageRecord = this->diskPage->Get(i);
			diskPageRecord.SetDeleteFlag(true);
			this->diskPage->Set(i, diskPageRecord);

			break;
		}

		if (this->diskPage->Get(i).GetKey() > key)
		{
			if (i == 0)
			{
				areaRecord = this->SetToDeleteInOverflow(key, this->basePointer);
			}
			else
			{
				areaRecord = this->SetToDeleteInOverflow(key, this->diskPage->Get(i).GetPointer());
			}

			return areaRecord;
		}
	}

	this->WriteDiskPageToPrimary(page);
	return areaRecord;
}

void Dbms::Read(const unsigned key)
{
	const auto areaRecord = this->FindAreaRecord(key);

	if (areaRecord.GetKey() != 0)
	{
		std::cout << "Data: " << 
			"(" << areaRecord.GetRecord().GetInput().first << ", " << areaRecord.GetRecord().GetInput().second << ")" << std::endl;
		std::cout << "Pointer: " << areaRecord.GetPointer() << std::endl;
		std::cout << "DeleteFlag: " << areaRecord.GetDeleteFlag() << std::endl;
	}
	else
	{
		std::cout << "Requested record with key " << key << " doesn't exist!" << std::endl;
	}
}

void Dbms::PrintAll() const
{
	std::cout << std::endl;

	dbmsUtils->PrintIndexRecords();
	dbmsUtils->PrintPrimaryRecords();
	dbmsUtils->PrintOverflowRecords();
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
	delete this->dbmsUtils;

	delete this->indexArea;
	delete this->primaryArea;
	delete this->overflowArea;

	delete this->diskPage;
}
