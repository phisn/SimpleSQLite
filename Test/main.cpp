#include "DatabaseCore/DatabaseCore.h"
#include <iostream>

int main()
{
	Database::Database db("test_db.db");

	if (!db)
	{
		std::cout << "Failed to open database";
		return 0;
	}

	Database::Statement<SQLiteInt, SQLiteString> statement(
		&db,
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
