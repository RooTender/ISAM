#pragma once
#include "IndexArea.h"

class DiskPage
{
	uint32_t blockingFactor{};
	size_t fullAreaRecordSize{}, indexAreaRecordSize{};
	uint32_t* diskOperations = nullptr;

public:
	explicit DiskPage(uint32_t* diskOperationsCounter);

	void ClearDiskPage() const;
	uint32_t GetDiskPage(uint32_t key);
	void SetDiskPage(uint32_t pageNo);

	uint32_t BinarySearchPage(const IndexArea& indexArea, uint32_t key);
};

