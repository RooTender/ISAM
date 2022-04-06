#include "DiskPage.h"

DiskPage::DiskPage(uint32_t* diskOperationsCounter) : diskOperations(diskOperationsCounter)
{
}

uint32_t DiskPage::BinarySearchPage(const IndexArea& indexArea, const uint32_t key)
{
	const size_t indexPageSize = blockingFactor * fullAreaRecordSize / indexAreaRecordSize * indexAreaRecordSize;
	const size_t pages = indexArea.GetLength() / indexPageSize + 1;
	uint32_t left = 0, right = pages, pointer = 0;

	const auto indexPage = new uint32_t[indexPageSize];
	size_t indexPageLength = 0;

	// External binary search
	bool found = false;
	while (!found)
	{
		uint32_t last;

		this->diskOperations++;
		auto file = std::ifstream(indexArea.GetFilename(), std::ifstream::binary);

		pointer = (left + right) / 2;

		// First page record
		const uint32_t first = indexArea.GetRecord(file, pointer * indexPageSize);

		// Last page record
		bool fullPageWasRead = true;
		if ((pointer + 1) * indexPageSize - indexAreaRecordSize > indexArea.GetLength())
		{
			last = indexArea.GetRecord(file, indexArea.GetLength() - indexAreaRecordSize);
			fullPageWasRead = false;
		}
		else
		{
			last = indexArea.GetRecord(file, (pointer + 1) * indexPageSize - indexAreaRecordSize);
		}

		if (first <= key && last >= key || !fullPageWasRead || pointer == 0 && first > key)
		{
			const unsigned page = pointer * indexPageSize;
			file.seekg(page, std::basic_ifstream<char>::beg);
			do
			{
				file.read(reinterpret_cast<char*>(&indexPage[indexPageLength]), indexAreaRecordSize);
			} while (indexPageLength < indexPageSize && indexPage[indexPageLength++] != last);

			found = true;
		}
		else
		{
			if (key < first)
			{
				right = pointer;
			}
			else
			{
				left = pointer;
			}
		}

		file.close();
	}

	left = 0;
	right = indexPageLength;

	// Internal binary search
	while (left != right)
	{
		pointer = (left + right) / 2;

		if (key < indexPage[pointer])
		{
			right = pointer;
		}
		else
		{
			left = pointer;

			right = right - left != 1 ? right : right - 1; // fix stuck on 0.5 issue
		}
	}
	delete[] indexPage;

	return pointer + 1; // returns position, not index
}
