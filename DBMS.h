#pragma once
// ReSharper disable once CppUnusedIncludeDirective
#include "FileUtils.h"
#include "Areas.h"
#include "AreaRecord.h"
#include <iostream>


class Dbms final
{
	PrimaryArea* primaryArea = nullptr;
	OverflowArea* overflowArea = nullptr;
	IndexArea* indexArea = nullptr;

	AreaRecord* diskPage = nullptr;

	std::string basePointerFilename;

	const size_t mainRecordSize = sizeof(uint32_t) * 2 + sizeof(double) * 2 + sizeof(bool);
	const size_t indexRecordSize = sizeof(uint32_t);
	
	uint32_t blockingFactor;
	uint32_t diskOperations = 0;
	uint32_t basePointer = 0;

	double alpha, maxOverflowOccupation;
	
	void BackupBasePointer();
	void RecreateAreas(bool backup) const;
	void UpdateAreasLength() const;

	uint32_t GetIndexRecord(std::ifstream& file, uint32_t index) const;
	
	static AreaRecord GetAreaRecord(std::ifstream& file);
	AreaRecord GetAreaRecord(std::ifstream& file, uint32_t index) const;
	static void AppendAreaRecord(std::ofstream& file, const AreaRecord& record);
	void SetAreaRecord(std::ofstream& file, AreaRecord record, uint32_t index) const;

	bool IsNextRecordOnCurrentPage(const uint32_t& pageAnchor, const uint32_t& pointerToNextRecord) const;
	void GetRawPage(const std::string& filename, const uint32_t& index, AreaRecord* dest);
	void AppendRawPage(const std::string& filename, const AreaRecord* src);
	void UpdateRawPage(const std::string& string, const AreaRecord* auxPage, uint32_t anchor);
	void AppendPageWithAlphaCorrection(uint32_t& currentOccupation);

	std::pair<uint32_t, AreaRecord> FindAreaRecordInOverflow(uint32_t key, uint32_t pointer);
	AreaRecord FindAreaRecord(uint32_t key);
	bool UpdateAreaRecordInOverflow(uint32_t key, Record data, uint32_t startPointer);

	AreaRecord SetToDeleteInOverflow(uint32_t key, uint32_t pointer);

	void InsertToOverflow(uint32_t key, Record record, uint32_t& startPointer);
	void InsertToBasePointer(uint32_t key, Record record);
	void InsertToPrimary(uint32_t key, Record record);

	void FillRecordsFromOverflow(size_t& pointer, size_t& index);
	void GetPageToReorganize(uint32_t& lastPosition, uint32_t& lastPointer);
	void Reorganize();

public:
	Dbms(uint32_t blockingFactor, double alpha, double maxOverflowOccupation,
		const std::string& primaryAreaFilename, const std::string& overflowAreaFilename, const std::string& indexAreaFilename);
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
