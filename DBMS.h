#pragma once
// ReSharper disable once CppUnusedIncludeDirective
#include "FileUtils.h"
#include "DbmsUtils.h"
#include "DiskPage.h"
#include <iostream>


class Dbms final
{
	DbmsUtils* dbmsUtils = nullptr;

	PrimaryArea* primaryArea = nullptr;
	OverflowArea* overflowArea = nullptr;
	IndexArea* indexArea = nullptr;

	DiskPage* diskPage = nullptr;

	std::string basePointerFilename;

	double alpha;
	double maxOverflowOccupation;

	const unsigned areaRecordSize = sizeof(unsigned) * 2 + sizeof(double) * 2 + sizeof(bool);
	const unsigned indexRecordSize = sizeof(unsigned);
	
	unsigned blockingFactor;
	unsigned diskOperations = 0;
	unsigned basePointer = 0;

	void InitializeBasePointer();
	void UpdateBasePointer();

	void RecreateAreas(bool backup) const;
	void UpdateAreasLength() const;

	void OverrideAreaRecordOnPage(std::ofstream& file, AreaRecord record, unsigned index) const;

	bool IsNextRecordOnCurrentPage(const unsigned& pageAnchor, const unsigned& pointerToNextRecord) const;
	void FillWithAreaRecords(const std::string& filename, const unsigned& index, AreaRecord* dest);
	void AppendRawPage(const std::string& filename, const AreaRecord* src);
	void UpdateRawPage(const std::string& string, const AreaRecord* auxPage, unsigned anchor);
	void AppendPageWithAlphaCorrection(unsigned& currentOccupation);

	std::pair<unsigned, AreaRecord> FindAreaRecordInOverflow(unsigned key, unsigned pointer);
	AreaRecord FindAreaRecord(unsigned key);
	bool UpdateAreaRecordInOverflow(unsigned key, Record data, unsigned startPointer);

	unsigned DeterminePageNumber(unsigned key);
	void LoadPrimaryToDiskPage(unsigned pageNumber);
	void WriteDiskPageToPrimary(unsigned pageNumber);

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

	void PrintAll() const;

	void PrintDiskOperations(bool resetCounter);

	~Dbms();
};
