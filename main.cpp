#include "DBMS.h"
#include "DBMSTester.h"

int main()
{
	DBMS dbms = DBMS(4, 0.5, 0.8);
	Record test = Record(1, 2);

	dbms.insert(5, test);
	dbms.printAll();

	dbms.insert(4, test);
	dbms.printAll();

	dbms.insert(3, test);
	dbms.printAll();

	dbms.insert(2, test);
	dbms.printAll();

	dbms.insert(1, test);
	dbms.printAll();

	return 0;
}