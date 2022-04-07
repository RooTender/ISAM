#include "AreaRecord.h"

AreaRecord::AreaRecord() : id(0), data(Record(0, 0)), next(0), deleteFlag(false)
{
}

AreaRecord::AreaRecord(const unsigned key, Record data, const unsigned pointer, const bool deleteFlag):
	id(key), data(std::move(data)), next(pointer), deleteFlag(deleteFlag)
{
}

void AreaRecord::SetKey(const unsigned& key)
{
	this->id = key;
}

void AreaRecord::SetRecord(const Record& record)
{
	this->data = record;
}

void AreaRecord::SetPointer(const unsigned& pointer)
{
	this->next = pointer;
}

void AreaRecord::SetDeleteFlag(const bool toDelete)
{
	this->deleteFlag = toDelete;
}

unsigned AreaRecord::GetKey() const
{
	return this->id;
}

Record AreaRecord::GetRecord() const
{
	return this->data;
}

unsigned AreaRecord::GetPointer() const
{
	return this->next;
}

bool AreaRecord::GetDeleteFlag() const
{
	return this->deleteFlag;
}
