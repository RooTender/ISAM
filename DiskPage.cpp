#include "DiskPage.h"

DiskPage::DiskPage(const size_t diskPageSize) : page(new AreaRecord[diskPageSize]), size(diskPageSize)
{
}

AreaRecord DiskPage::Get(const unsigned long long index) const
{
	return this->page[index];
}

void DiskPage::Set(const unsigned long long index, const AreaRecord& areaRecord) const
{
	this->page[index] = areaRecord;
}

void DiskPage::Clear() const
{
	for (size_t i = 0; i < size; ++i)
	{
		this->page[i] = AreaRecord();
	}
}

DiskPage::~DiskPage()
{
	delete[] this->page;
}
