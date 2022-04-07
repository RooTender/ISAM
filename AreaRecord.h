#pragma once
#include "Record.h"

class AreaRecord
{
	unsigned id;
	Record data;
	unsigned next;
	bool deleteFlag;

public:
	AreaRecord();
	explicit AreaRecord(unsigned key, Record data, unsigned pointer, bool deleteFlag);

	void SetKey(const unsigned& key);
	void SetRecord(const Record& record);
	void SetPointer(const unsigned& pointer);
	void SetDeleteFlag(bool toDelete);

	unsigned GetKey() const;
	Record GetRecord() const;
	unsigned GetPointer() const;
	bool GetDeleteFlag() const;
};

