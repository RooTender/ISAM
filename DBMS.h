#pragma once
#include "Record.h"
#include <string>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <errno.h>

struct AreaRecord
{
	AreaRecord() {}
	AreaRecord(uint32_t key, Record data, uint32_t pointer, bool deleteFlag)
	{
		this->key = key;
		this->data = data;
		this->pointer = pointer;
		this->deleteFlag = deleteFlag;
	}

	uint32_t key = 0;
	Record data = Record(0, 0);
	uint32_t pointer = 0;
	bool deleteFlag = false;
};

class DBMS
{
private:
	struct Area
	{
		const std::string overflow	= "./overflow.bin";
		const std::string index		= "./index.bin";
		const std::string primary	= "./primary.bin";

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

	uint32_t
		blockingFactor,
		diskOperations,
		basePointer, overflowPointer;// , diskPagePointer;
	double alpha, maxOverflowOccupation;

	bool isFileEmpty(std::ofstream& file);
	size_t getFileLength(std::ifstream& file);
	size_t getFileLength(std::ofstream& file);
	void moveCursorToTheNextAreaRecord(std::ofstream& file);

	uint32_t getIndexRecord(std::ifstream& file, uint32_t index);
	uint32_t binarySearchPage(uint32_t key);

	AreaRecord getAreaRecord(std::ifstream& file);
	AreaRecord getAreaRecord(std::ifstream& file, uint32_t index);
	void appendAreaRecord(std::ofstream& file, AreaRecord record);
	void appendAreaRecordWithAlphaCorrection(std::ofstream& primary, std::ofstream& indexes, AreaRecord record, uint32_t& pageCounter);
	void appendAreaRecordsFromOverflow(std::ofstream& primary, std::ifstream& overflow, std::ofstream& indexes, uint32_t startIndex, uint32_t& pageCounter);
	void setAreaRecord(std::ofstream& file, AreaRecord record, uint32_t index);

	uint32_t getDiskPage(uint32_t key);
	void setDiskPage(uint32_t pageNo);

	//std::pair<AreaRecord, uint32_t> getRecordWithIndexFromOverflow(uint32_t key, uint32_t pointingRecordIndex);
	void setDeleteFlagInOverflow(uint32_t key, bool deleteFlag);
	
	// Returns pointer from overflow area
	uint32_t insertToOverflow(uint32_t key, Record record, uint32_t pointer);
	void insertToBasePointer(uint32_t key, Record record);
	void insertToPrimary(uint32_t key, Record record);

	void reorganize();

public:
	DBMS(uint32_t blockingFactor, double alpha, double maxOverflowOccupation);

	void update(bool forceUpdate = false);
	void insert(uint32_t key, Record record);
	void remove(uint32_t key);
	void read(uint32_t key);

	void printIndex();
	void printPrimary();
	void printDiskPage();
	void printOverflow();
	void printAll();

	~DBMS();
};

