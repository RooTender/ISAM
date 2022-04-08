#pragma once
#include "Areas.h"
#include "AreaRecord.h"
#include <iostream>
#include <string>


class DbmsUtils
{
	IndexArea* indexArea = nullptr;
	PrimaryArea* primaryArea = nullptr;
	OverflowArea* overflowArea = nullptr;

	size_t areaRecordSize;
	size_t indexRecordSize;

	static void PrintHeader(const std::string& header, bool indexAreaAttributes);
	static void PrintAreaRecord(const AreaRecord& areaRecord);
	void PrintAreaRecords(const std::string& filename) const;

	static AreaRecord ReadAreaRecord(std::ifstream& file);

public:
	DbmsUtils(IndexArea* indexArea, PrimaryArea* primaryArea, OverflowArea* overflowArea,
	          size_t areaRecordSize, size_t indexRecordSize);

	unsigned ReadIndexRecord(std::ifstream& file, unsigned index) const;
	static void WriteIndexRecord(std::ofstream& file, unsigned key);

	AreaRecord ReadAreaRecord(std::ifstream& file, unsigned index) const;
	static void WriteAreaRecord(std::ofstream& file, const AreaRecord& record);

	void PrintIndexRecords() const;
	void PrintOverflowRecords() const;
	void PrintPrimaryRecords() const;
};

