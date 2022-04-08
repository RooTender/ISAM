// ReSharper disable CppClangTidyConcurrencyMtUnsafe
#include "Dbms.h"
#include "Record.h"
#include <random>

void PrintLegend()
{
	std::cout << " == Instructions == " << std::endl;
	std::cout << "Type 'I' <key> to insert new record" << std::endl;
	std::cout << "Type 'U' <key> to update record" << std::endl;
	std::cout << "Type 'K' <old key> <new key> to update key of the record" << std::endl;
	std::cout << "Type 'G' <amount> generate random keys" << std::endl;
	std::cout << "Type 'D' <key> to delete record" << std::endl;
	std::cout << "Type 'R' <key> to read record" << std::endl;
	std::cout << "Type 'P' to print files data" << std::endl;
	std::cout << "Type 'O' to print and reset disk operations count" << std::endl;
	std::cout << "Type 'X' to reorganize files" << std::endl;
	std::cout << "Type 'E' to exit program" << std::endl;
}

int main()
{
	srand(static_cast<unsigned>(time(nullptr))); // NOLINT(cert-msc51-cpp)

	unsigned blockingFactor;
	double alpha, maxOverflowOccupation;

	std::cout << "Hubert Lewandowski 180348" << std::endl;
	std::cout << "Blocking factor: ";
	std::cin >> blockingFactor;
	std::cout << "Alpha: ";
	std::cin >> alpha;
	std::cout << "Max overflow occupation: ";
	std::cin >> maxOverflowOccupation;

	auto dbms = Dbms(blockingFactor, alpha, maxOverflowOccupation,
		"./primary.bin", "./overflow.bin", "./index.bin");
	std::cout << std::endl;
	std::cout << "DBMS initialized!" << std::endl;
	std::cout << "Interactive console initialized!" << std::endl;
	std::cout << std::endl;
	PrintLegend();

	auto exit = false;
	while (!exit)
	{
		std::cout << std::endl;

		char option;
		std::cin >> option;
		option = static_cast<char>(std::toupper(option));

		unsigned key;

		if (option == 'I')
		{
			std::cin >> key;
			dbms.Insert(key, Record(rand() % 10, rand() % 10));
		}

		else if (option == 'U')
		{
			std::cin >> key;
			dbms.UpdateRecord(key, Record(rand() % 10, rand() % 10));
		}

		else if (option == 'K')
		{
			unsigned newKey;

			std::cin >> key;
			std::cin >> newKey;
			dbms.UpdateKey(key, newKey);
		}

		else if (option == 'G')
		{
			size_t amount;
			std::cin >> amount;

			std::cout << "Keys: ";
			for (size_t i = 0; i < amount; ++i)
			{
				const unsigned newKey = rand() % amount + 1;
				std::cout << newKey << ", ";
				dbms.Insert(newKey, Record(rand() % 10, rand() % 10));
			}
			std::cout << std::endl;
		}

		else if (option == 'D')
		{
			std::cin >> key;
			dbms.Remove(key);
		}

		else if (option == 'R')
		{
			std::cin >> key;
			dbms.Read(key);
		}

		else if (option == 'P')
		{
			dbms.PrintAll();
		}

		else if (option == 'O')
		{
			dbms.PrintDiskOperations(true);
		}

		else if (option == 'X')
		{
			dbms.UpdateFileStructure(true);
		}

		else if (option == 'E')
		{
			exit = true;
		}

		else
		{
			std::cout << "Unknown command!" << std::endl;
			PrintLegend();
		}

		dbms.PrintDiskOperations(false);
		std::cout << std::endl;
	}

	return 0;
}
