#include "DbmsUtils.h"

void DbmsUtils::PrintHeader(const std::string& header, const bool indexAreaAttributes)
{
	std::cout << " === " << header << " === " << std::endl;

	if (indexAreaAttributes)
	{
		std::cout << "key" << std::endl;
	}
	else
	{
		std::cout << "key\tdata\t\tpointer\tdelete" << std::endl;
	}
}

void DbmsUtils::PrintIndexRecords() const
{
	this->PrintHeader("Index", true);

	const size_t length = this->indexArea->GetLength() / indexRecordSize;
	auto file = std::ifstream(this->indexArea->GetFilename(), std::ifstream::binary);

	for (size_t i = 0; i < length; ++i)
	{
		const auto key = this->ReadIndexRecord(file, static_cast<unsigned>(i));
		std::cout << key << std::endl;
	}
	std::cout << std::endl;

	file.close();
}

void DbmsUtils::PrintAreaRecord(const AreaRecord& areaRecord)
{
	std::cout << areaRecord.GetKey() <<
		"\t(" << areaRecord.GetRecord().GetInput().first << ", " << areaRecord.GetRecord().GetInput().second << ")\t" <<
		"\t" << areaRecord.GetPointer() <<
		"\t" << areaRecord.GetDeleteFlag() << std::endl;
}

void DbmsUtils::PrintAreaRecords(const std::string& filename) const
{
	auto file = std::ifstream(filename, std::ifstream::binary);
	const size_t length = FileUtils::GetFileLength(file) / areaRecordSize;

	for (size_t i = 0; i < length; ++i)
	{
		const auto areaRecord = this->ReadAreaRecord(file, static_cast<unsigned>(i));
		this->PrintAreaRecord(areaRecord);
	}
	std::cout << std::endl;

	file.close();
}

AreaRecord DbmsUtils::ReadAreaRecord(std::ifstream& file)
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

DbmsUtils::DbmsUtils(IndexArea* indexArea, PrimaryArea* primaryArea, OverflowArea* overflowArea,
                     const size_t areaRecordSize, const size_t indexRecordSize) :
	indexArea(indexArea), primaryArea(primaryArea), overflowArea(overflowArea),
	areaRecordSize(areaRecordSize), indexRecordSize(indexRecordSize)
{
}


unsigned DbmsUtils::ReadIndexRecord(std::ifstream& file, const unsigned index) const
{
	unsigned record = 0;
	const unsigned selectedPage = index * static_cast<unsigned>(indexRecordSize);

	file.seekg(selectedPage, std::ifstream::beg);
	file.read(reinterpret_cast<char*>(&record), static_cast<unsigned>(indexRecordSize));

	return record;
}

void DbmsUtils::WriteIndexRecord(std::ofstream& file, unsigned key)
{
	file.write(reinterpret_cast<char*>(&key), sizeof(unsigned));
}

AreaRecord DbmsUtils::ReadAreaRecord(std::ifstream& file, const unsigned index) const
{
	const std::streampos currentPosition = file.tellg();
	const auto selectedPage = index * areaRecordSize;

	file.seekg(static_cast<unsigned>(selectedPage), std::ifstream::beg);
	AreaRecord areaRecord = this->ReadAreaRecord(file);

	file.seekg(currentPosition);

	return areaRecord;
}

void DbmsUtils::WriteAreaRecord(std::ofstream& file, const AreaRecord& record)
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

void DbmsUtils::PrintOverflowRecords() const
{
	this->PrintHeader("Overflow", false);
	this->PrintAreaRecords(this->overflowArea->GetFilename());
}

void DbmsUtils::PrintPrimaryRecords() const
{
	this->PrintHeader("Primary", false);
	this->PrintAreaRecords(this->primaryArea->GetFilename());
}
