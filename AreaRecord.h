#pragma once
#include "Record.h"

class AreaRecord
{
	uint32_t key;
	Record data;
	uint32_t pointer;
	bool deleteFlag;

public:
	AreaRecord();

	explicit AreaRecord(const uint32_t key, Record data, const uint32_t pointer, const bool deleteFlag) :
		key(key), data(std::move(data)), pointer(pointer), deleteFlag(deleteFlag)
	{
	}

	void SetKey(const uint32_t& key);
	void SetRecord(const Record& record);
	void SetPointer(const uint32_t& pointer);
	void SetDeleteFlag(bool toDelete);

	uint32_t GetKey() const;
	Record GetRecord() const;
	uint32_t GetPointer() const;
	bool GetDeleteFlag() const;
};

