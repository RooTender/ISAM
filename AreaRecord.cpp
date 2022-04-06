#include "AreaRecord.h"

AreaRecord::AreaRecord() : key(0), data(Record(0, 0)), pointer(0), deleteFlag(false)
{
}

void AreaRecord::SetKey(const uint32_t& key)
{
	this->key = key;
}

void AreaRecord::SetRecord(const Record& record)
{
	this->data = record;
}

void AreaRecord::SetPointer(const uint32_t& pointer)
{
	this->pointer = pointer;
}

void AreaRecord::SetDeleteFlag(const bool toDelete)
{
	this->deleteFlag = toDelete;
}

uint32_t AreaRecord::GetKey() const
{
	return this->key;
}

Record AreaRecord::GetRecord() const
{
	return this->data;
}

uint32_t AreaRecord::GetPointer() const
{
	return this->pointer;
}

bool AreaRecord::GetDeleteFlag() const
{
	return this->deleteFlag;
}
