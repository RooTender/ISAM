#pragma once
// ReSharper disable once CppUnusedIncludeDirective
#include "FileUtils.h"
#include "Record.h"
#include <string>
#include <iostream>

struct AreaRecord
{
	AreaRecord() = default;

	AreaRecord(const uint32_t key, Record data, const uint32_t pointer, const bool deleteFlag) :
		key(key), data(std::move(data)), pointer(pointer), deleteFlag(deleteFlag)
	{
	}

	uint32_t key = 0;
	Record data = Record(0, 0);
	uint32_t pointer = 0;
	bool deleteFlag = false;
};

class Dbms final
{
	struct Area
	{
		const std::string overflow = "./overflow.bin";
		const std::string index = "./index.bin";
		const std::string primary = "./primary.bin";

		struct Length
		{
			size_t overflow = 0;
			size_t index = 0;
			size_t primary = 0;
		} length;
	} area;

	const size_t mainRecordSize = sizeof(uint32_t) * 2 + sizeof(double) * 2 + sizeof(bool);
	const size_t indexRecordSize = sizeof(uint32_t);
	AreaRecord* diskPage{};

	uint32_t blockingFactor;
	uint32_t diskOperations = 0;
	uint32_t basePointer = 0;

	double alpha, maxOverflowOccupation;

	void UpdateLengthData();
	void BackupBasePointer();
	void RecreateAreas(bool backup) const;

	uint32_t GetIndexRecord(std::ifstream& file, uint32_t index) const;
	uint32_t BinarySearchPage(uint32_t key);

	static AreaRecord GetAreaRecord(std::ifstream& file);
	AreaRecord GetAreaRecord(std::ifstream& file, uint32_t index) const;
	static void AppendAreaRecord(std::ofstream& file, AreaRecord record);
	void SetAreaRecord(std::ofstream& file, AreaRecord record, uint32_t index) const;

	bool IsNextRecordOnCurrentPage(const uint32_t& pageAnchor, const uint32_t& pointerToNextRecord) const;
	void GetRawPage(const std::string& filename, const uint32_t& index, AreaRecord* dest);
	void AppendRawPage(const std::string& filename, const AreaRecord* src);
	void UpdateRawPage(const std::string& string, const AreaRecord* auxPage, uint32_t anchor);
	void AppendPageWithAlphaCorrection(uint32_t& currentOccupation);

	std::pair<uint32_t, AreaRecord> FindAreaRecordInOverflow(uint32_t key, uint32_t pointer);
	AreaRecord FindAreaRecord(uint32_t key);
	bool UpdateAreaRecordInOverflow(uint32_t key, Record data, uint32_t startPointer);

	void ClearDiskPage() const;
	uint32_t GetDiskPage(uint32_t key);
	void SetDiskPage(uint32_t pageNo);

	AreaRecord SetToDeleteInOverflow(uint32_t key, uint32_t pointer);

	void InsertToOverflow(uint32_t key, Record record, uint32_t& startPointer);
	void InsertToBasePointer(uint32_t key, Record record);
	void InsertToPrimary(uint32_t key, Record record);

	void FillRecordsFromOverflow(size_t& pointer, size_t& index);
	void GetPageToReorganize(uint32_t& lastPosition, uint32_t& lastPointer);
	void Reorganize();

public:
	Dbms(uint32_t blockingFactor, double alpha, double maxOverflowOccupation);
	Dbms(const Dbms&) = default;
	Dbms(Dbms&&) = default;
	Dbms& operator=(Dbms&& other) = delete;
	Dbms& operator=(const Dbms&) = delete;

	void UpdateFileStructure(bool forceUpdate = false);
	void Insert(uint32_t key, Record record);
	void UpdateRecord(uint32_t key, Record record);
	void UpdateKey(uint32_t oldKey, uint32_t newKey);
	AreaRecord Remove(uint32_t key);
	void Read(uint32_t key);

	void PrintIndex() const;
	void PrintPrimary() const;
	void PrintDiskPage() const;
	void PrintOverflow() const;
	void PrintAll() const;

	void PrintDiskOperations(bool resetCounter);

	~Dbms();
};
