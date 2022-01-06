#pragma once
#include "Record.h"
#include <string>
#include <iostream>

struct AreaRecord
{
	AreaRecord()
	{
	}

	AreaRecord(const uint32_t key, Record data, const uint32_t pointer, const bool deleteFlag) :
		key(key), data(std::move(data)), pointer(pointer), deleteFlag(deleteFlag)
	{
	}

	uint32_t key = 0;
	Record data = Record(0, 0);
	uint32_t pointer = 0;
	bool deleteFlag = false;
};

using page = uint32_t;

class Dbms
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
	AreaRecord* diskPage;

	uint32_t blockingFactor;
	uint32_t diskOperations = 0;
	uint32_t basePointer = 0;

	double alpha, maxOverflowOccupation;

	static size_t getFileLength(std::ifstream& file);
	static size_t getFileLength(std::ofstream& file);
	void updateLengthData();
	void backupBasePointer();
	void recreateAreas(bool backup) const;

	uint32_t getIndexRecord(std::ifstream& file, uint32_t index) const;
	uint32_t binarySearchPage(uint32_t key);

	static AreaRecord getAreaRecord(std::ifstream& file);
	AreaRecord getAreaRecord(std::ifstream& file, uint32_t index) const;
	static void appendAreaRecord(std::ofstream& file, AreaRecord record);
	void setAreaRecord(std::ofstream& file, AreaRecord record, uint32_t index) const;

	bool isNextRecordOnCurrentPage(const uint32_t& pageAnchor, const uint32_t& pointerToNextRecord) const;
	void getRawPage(const std::string& filename, const uint32_t& index, AreaRecord* dest);
	void appendRawPage(const std::string& filename, const AreaRecord* src);
	void updateRawPage(const std::string& string, const AreaRecord* auxPage, uint32_t anchor);
	void appendPageWithAlphaCorrection(uint32_t& currOccupation);

	std::pair<uint32_t, AreaRecord> findAreaRecordInOverflow(uint32_t key, uint32_t pointer);
	AreaRecord findAreaRecord(uint32_t key);
	bool updateAreaRecordInOverflow(uint32_t key, Record data, uint32_t startPointer);

	void clearDiskPage() const;
	uint32_t getDiskPage(uint32_t key);
	void setDiskPage(uint32_t pageNo);

	AreaRecord setToDeleteInOverflow(uint32_t key, uint32_t pointer);

	void insertToOverflow(uint32_t key, Record record, uint32_t& startPointer);
	void insertToBasePointer(uint32_t key, Record record);
	void insertToPrimary(uint32_t key, Record record);

	void fillRecordsFromOverflow(size_t& pointer, size_t& index);
	void getPageToReorganize(uint32_t& lastPosition, uint32_t& lastPointer);
	void reorganize();

public:
	Dbms(uint32_t blockingFactor, double alpha, double maxOverflowOccupation);

	void updateFileStructure(bool forceUpdate = false);
	void insert(uint32_t key, Record record);
	void updateRecord(uint32_t key, Record record);
	void updateKey(uint32_t oldKey, uint32_t newKey);
	AreaRecord remove(uint32_t key);
	void read(uint32_t key);

	void printIndex() const;
	void printPrimary() const;
	void printDiskPage() const;
	void printOverflow() const;
	void printAll() const;

	void printDiskOperations(bool resetCounter);

	~Dbms();
};
