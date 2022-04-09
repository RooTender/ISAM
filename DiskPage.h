#pragma once
#include "AreaRecord.h"

class DiskPage
{
	AreaRecord* page = nullptr;
	size_t size = 0;

public:
	DiskPage() = delete;
	DiskPage(const DiskPage&) = default;
	DiskPage(DiskPage&&) = default;
	DiskPage& operator=(DiskPage&& other) = delete;
	DiskPage& operator=(const DiskPage&) = delete;

	explicit DiskPage(size_t diskPageSize);

	AreaRecord Get(unsigned long long index) const;
	void Set(unsigned long long index, const AreaRecord& areaRecord) const;

	void Clear() const;

	~DiskPage();
};

