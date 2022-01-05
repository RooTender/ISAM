#include "DBMS.h"

int main()
{
	auto dbms = Dbms(4, 0.5, 0.8);
	const auto test = Record(1, 2);

	dbms.insert(3, test);
	dbms.printAll();

	dbms.insert(2, test);
	dbms.printAll();

	dbms.insert(1, test);
	dbms.printAll();

	dbms.insert(4, test);
	dbms.printAll();

	dbms.printAll();
	dbms.read(1);

	return 0;
}
