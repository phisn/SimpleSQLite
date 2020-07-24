#include "DatabaseCore.h"

#ifdef _DEBUG
#include <iostream>
#endif

namespace Database
{
#ifdef _DEBUG
std::function<void(const char*)> OnDatabaseCoreFailure =
	[](const char* error_message)
	{
		std::cout << "DatabaseCore failure: " << error_message << std::endl;
	};
std::function<void(int, const char*)> OnSQLiteFailure =
	[](int error_code, const char* error_message)
	{
		std::cout << "SQLite failure: " << error_message << "(" << sqlite3_errstr(error_code) << ")" << std::endl;
	};
#else
	std::function<void(int, const char*)> OnSQLiteFailure;
#endif

	bool EnsureSQLiteStatusCode(int status_code, const char* message)
	{
		if (status_code != SQLITE_OK)
		{
			if (OnSQLiteFailure)
				OnSQLiteFailure(status_code, message);

			return false;
		}

		return true;
	}
}
