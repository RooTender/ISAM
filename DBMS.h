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

	double alpha;
	double maxOverflowOccupation;

	const unsigned fullAreaRecordSize = sizeof(unsigned) * 2 + sizeof(double) * 2 + sizeof(bool);
	const unsigned indexAreaRecordSize = sizeof(unsigned);
	
	unsigned blockingFactor;
	unsigned diskOperations = 0;
	unsigned basePointer = 0;
	
	void BackupBasePointer();
	void RecreateAreas(bool backup) const;
	void UpdateAreasLength() const;

	unsigned GetIndexRecord(std::ifstream& file, unsigned index) const;
	
	static AreaRecord GetAreaRecord(std::ifstream& file);
	AreaRecord GetAreaRecord(std::ifstream& file, unsigned index) const;
	static void AppendAreaRecord(std::ofstream& file, const AreaRecord& record);
	void SetAreaRecord(std::ofstream& file, AreaRecord record, unsigned index) const;

	bool IsNextRecordOnCurrentPage(const unsigned& pageAnchor, const unsigned& pointerToNextRecord) const;
	void GetRawPage(const std::string& filename, const unsigned& index, AreaRecord* dest);
	void AppendRawPage(const std::string& filename, const AreaRecord* src);
	void UpdateRawPage(const std::string& string, const AreaRecord* auxPage, unsigned anchor);
	void AppendPageWithAlphaCorrection(unsigned& currentOccupation);

	std::pair<unsigned, AreaRecord> FindAreaRecordInOverflow(unsigned key, unsigned pointer);
	AreaRecord FindAreaRecord(unsigned key);
	bool UpdateAreaRecordInOverflow(unsigned key, Record data, unsigned startPointer);

	void ClearDiskPage() const;
	unsigned BinarySearchPage(unsigned key);
	unsigned GetDiskPage(unsigned key);
	void SetDiskPage(unsigned pageNo);

	AreaRecord SetToDeleteInOverflow(unsigned key, unsigned pointer);

	void InsertToOverflow(unsigned key, Record record, unsigned& startPointer);
	void InsertToBasePointer(unsigned key, Record record);
	void InsertToPrimary(unsigned key, Record record);

	void FillRecordsFromOverflow(unsigned& pointer, unsigned& index);
	void GetPageToReorganize(unsigned& lastPosition, unsigned& lastPointer);
	void Reorganize();

public:
	Dbms(unsigned blockingFactor, double alpha, double maxOverflowOccupation,
		const std::string& primaryAreaFilename, const std::string& overflowAreaFilename, const std::string& indexAreaFilename);
	Dbms(const Dbms&) = default;
	Dbms(Dbms&&) = default;
	Dbms& operator=(Dbms&& other) = delete;
	Dbms& operator=(const Dbms&) = delete;

	void UpdateFileStructure(bool forceUpdate = false);
	void Insert(unsigned key, Record record);
	void UpdateRecord(unsigned key, Record record);
	void UpdateKey(unsigned oldKey, unsigned newKey);
	AreaRecord Remove(unsigned key);
	void Read(unsigned key);

	void PrintIndex() const;
	void PrintPrimary() const;
	void PrintDiskPage() const;
	void PrintOverflow() const;
	void PrintAll() const;

	void PrintDiskOperations(bool resetCounter);

	~Dbms();
};
