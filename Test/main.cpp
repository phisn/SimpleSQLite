#include "DatabaseCore/DatabaseCore.h"
#include <iostream>

void example_1(Database::Database& database);
void example_2(Database::Database& database);

/*

Output:
	example_1:
	1 : Hello World
	2 : Hello Two
	3 : Tree Hello
	example_2:
	name of 2 is Hello Two

*/

int main()
{
	Database::Database db("test_db.db");

	if (!db)
	{
		std::cout << "Failed to open database";
		return 0;
	}

	std::cout << "example_1:" << std::endl;
	example_1(db);

	std::cout << "example_2:" << std::endl;
	example_2(db);
}

void example_1(Database::Database& database)
{
	Database::Statement<SQLiteInt, SQLiteString> statement(
		&database,
		"SELECT id, name FROM test");

	for (auto row : statement)
	{
		std::cout << std::get<0>(row) << " : " << std::get<1>(row) << std::endl;
	}

	if (!statement)
	{
		std::cout << "Statement failed" << std::endl;
	}
}

void example_2(Database::Database& database)
{
	typedef std::tuple<SQLiteString> TestTuple;

	int test_id = 2;
	TestTuple result;

	if (!Database::Statement<TestTuple>(
			&database,
			"SELECT name FROM test WHERE id = ?",
			test_id).execute(result))
	{
		std::cout << "Statement failed" << std::endl;
		return;
	}

	std::cout << "name of " << test_id << " is " << std::get<0>(result) << std::endl;
}
