#pragma once

#include "sqlite3.h"

#include <cassert>
#include <functional>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

#pragma comment(lib, "sqlite3.lib")
#pragma warning(push)
#pragma warning(disable: 4267)

namespace Database
{
	typedef int64_t						SQLiteInt;    // integral
	typedef double						SQLiteReal;   // floating point
	typedef std::string					SQLiteString; // char*
	typedef std::vector<unsigned char>	SQLiteBlob;   // unsigned char*

	extern std::function<void(const char*)> OnDatabaseCoreFailure;
	extern std::function<void(int, const char*)> OnSQLiteFailure;
	bool EnsureSQLiteStatusCode(int status_code, const char* message);

	class SQLiteDatabase
	{
	public:
		SQLiteDatabase(std::string filename)
		{
			open(filename);
		}

		~SQLiteDatabase()
		{
			// null is a noop
			sqlite3_close(database);
		}

		operator bool()
		{
			return database != NULL;
		}

		std::string last_error_message() const
		{
			return sqlite3_errmsg(database);
		}

		int last_error_code() const
		{
			return sqlite3_errcode(database);
		}

		sqlite3* get_database() const
		{
			return database;
		}

	private:
		sqlite3* database;

		bool open(std::string filename)
		{
			return EnsureSQLiteStatusCode(
				sqlite3_open(filename.c_str(), &database),
				filename.c_str());
		}
	};

	template <typename T, typename Lazy = void>
	struct SQLiteStatementColumn
	{
	};

	template <typename T, typename Lazy>
	struct SQLiteStatementColumn<::std::optional<T>, Lazy>
	{
		template <typename Tuple, size_t Column = 0>
		static inline void Extract(Tuple& tuple, sqlite3_stmt* statement)
		{
			if (sqlite3_column_type(statement, Column) == SQLITE_NULL)
			{
				std::get<Column>(tuple).reset();
			}
			else
			{
				SQLiteStatementColumn<T>::template ExtractAs<Tuple, Column>(
					*std::get<Column>(tuple), statement);
			}
		}

		template <typename Tuple, size_t Column = 0>
		static inline int Bind(Tuple& tuple, sqlite3_stmt* statement)
		{
			if (std::get<Column>(tuple))
			{
				return SQLiteStatementColumn<T>::template BindAs(
					*std::get<Column>(tuple), Column, statement);
			}
			else
			{
				return sqlite3_bind_null(statement, Column + 1);
			}
		}
	};

	template <typename Lazy>
	struct SQLiteStatementColumn<std::nullopt_t, Lazy>
	{
		template <typename Tuple, size_t Column = 0>
		static inline int Bind(Tuple& tuple, sqlite3_stmt* statement)
		{
			return sqlite3_bind_null(statement, Column + 1);
		}
	};

	template <typename T>
	struct SQLiteStatementColumn<T, std::enable_if_t<std::is_integral_v<T>>>
	{
		template <typename Tuple, size_t Column = 0>
		static inline void Extract(Tuple& tuple, sqlite3_stmt* statement)
		{
			ExtractAs<Column>((SQLiteInt&)std::get<Column>(tuple), statement);
		}

		template <size_t Column = 0>
		static inline void ExtractAs(SQLiteInt& value, sqlite3_stmt* statement)
		{
			assert(sqlite3_column_type(statement, Column) != SQLITE_NULL);
			value = sqlite3_column_int64(statement, Column);
		}

		template <typename Tuple, size_t Column = 0>
		static inline int Bind(Tuple& tuple, sqlite3_stmt* statement)
		{
			return BindAs((SQLiteInt)std::get<Column>(tuple), Column, statement);
		}

		static inline int BindAs(SQLiteInt value, size_t Column, sqlite3_stmt* statement)
		{
			return sqlite3_bind_int64(statement, Column + 1, value);
		}
	};

	template <typename T>
	struct SQLiteStatementColumn<T, std::enable_if_t<std::is_floating_point_v<T>>>
	{
		template <typename Tuple, size_t Column = 0>
		static inline void Extract(Tuple& tuple, sqlite3_stmt* statement)
		{
			ExtractAs<Column>((SQLiteReal&)std::get<Column>(tuple), statement);
		}

		template <size_t Column = 0>
		static inline void ExtractAs(SQLiteReal& value, sqlite3_stmt* statement)
		{
			assert(sqlite3_column_type(statement, Column) != SQLITE_NULL);
			value = sqlite3_column_double(statement, Column);
		}

		template <typename Tuple, size_t Column = 0>
		static inline int Bind(Tuple& tuple, sqlite3_stmt* statement)
		{
			return BindAs((SQLiteReal)std::get<Column>(tuple), Column, statement);
		}

		static inline int BindAs(SQLiteReal value, size_t Column, sqlite3_stmt* statement)
		{
			return sqlite3_bind_double(statement, Column + 1, value);
		}
	};

	template <typename Lazy>
	struct SQLiteStatementColumn<SQLiteString, Lazy>
	{
		template <typename Tuple, size_t Column = 0>
		static inline void Extract(Tuple& tuple, sqlite3_stmt* statement)
		{
			ExtractAs<Column>(std::get<Column>(tuple), statement);
		}

		template <size_t Column = 0>
		static inline void ExtractAs(SQLiteString& value, sqlite3_stmt* statement)
		{
			assert(sqlite3_column_type(statement, Column) != SQLITE_NULL);
			value.assign((const char*)sqlite3_column_text(statement, Column));
		}

		template <typename Tuple, size_t Column = 0>
		static inline int Bind(Tuple& tuple, sqlite3_stmt* statement)
		{
			return BindAs(std::get<Column>(tuple), Column, statement);
		}

		static inline int BindAs(SQLiteString& value, size_t Column, sqlite3_stmt* statement)
		{
			return sqlite3_bind_text(
				statement, Column + 1, value.c_str(),
				value.size(),
				SQLITE_TRANSIENT);
		}
	};

	template <typename Lazy>
	struct SQLiteStatementColumn<const char*, Lazy>
	{
		template <typename Tuple, size_t Column = 0>
		static inline int Bind(Tuple& tuple, sqlite3_stmt* statement)
		{
			return BindAs(std::get<Column>(tuple), Column, statement);
		}

		static inline int BindAs(const char* value, size_t Column, sqlite3_stmt* statement)
		{
			return sqlite3_bind_text(
				statement,
				Column + 1, value,
				-1,
				SQLITE_TRANSIENT);
		}
	};

	template <typename Lazy>
	struct SQLiteStatementColumn<char*, Lazy>
		:
		public SQLiteStatementColumn<const char*>
	{
		template <typename Tuple, size_t Column = 0>
		static inline void Extract(Tuple& tuple, sqlite3_stmt* statement)
		{
			ExtractAs<Column>(std::get<Column>(tuple), statement);
		}

		template <size_t Column = 0>
		static inline void ExtractAs(char* value, sqlite3_stmt* statement)
		{
			assert(sqlite3_column_type(statement, Column) != SQLITE_NULL);
			memcpy(value,
				(const void*)sqlite3_column_text(statement, Column),
				sqlite3_column_bytes(statement, Column));
		}
	};

	template <typename Lazy>
	struct SQLiteStatementColumn<SQLiteBlob, Lazy>
	{
		template <typename Tuple, size_t Column = 0>
		static inline void Extract(Tuple& tuple, sqlite3_stmt* statement)
		{
			ExtractAs<Column>(std::get<Column>(tuple), statement);
		}

		template <size_t Column = 0>
		static inline void ExtractAs(SQLiteBlob& value, sqlite3_stmt* statement)
		{
			assert(sqlite3_column_type(statement, Column) != SQLITE_NULL);
			const char* data = (const char*)sqlite3_column_blob(statement, Column);
			value.assign(data, data + sqlite3_column_bytes(statement, Column));
		}

		template <typename Tuple, size_t Column = 0>
		static inline int Bind(Tuple& tuple, sqlite3_stmt* statement)
		{
			return BindAs(std::get<Column>(tuple), Column, statement);
		}

		static inline int BindAs(SQLiteBlob& value, size_t Column, sqlite3_stmt* statement)
		{
			return sqlite3_bind_blob(
				statement, Column + 1,
				(const void*)&value[0],
				value.size(),
				SQLITE_TRANSIENT);
		}
	};

	template <typename Lazy>
	struct SQLiteStatementColumn<const unsigned char*, Lazy>
	{
		template <typename Tuple, size_t Column = 0>
		static inline int Bind(Tuple& tuple, sqlite3_stmt* statement)
		{
			return BindAs(std::get<Column>(tuple), Column, statement);
		}

		static inline int BindAs(const unsigned char* value, size_t Column, sqlite3_stmt* statement)
		{
			return sqlite3_bind_blob(
				statement,
				Column + 1, (const void*)value,
				strlen((const char*)value),
				SQLITE_TRANSIENT);
		}
	};

	template <typename Lazy>
	struct SQLiteStatementColumn<unsigned char*, Lazy>
		:
		public SQLiteStatementColumn<const unsigned char*>
	{
		template <typename Tuple, size_t Column = 0>
		static inline void Extract(Tuple& tuple, sqlite3_stmt* statement)
		{
			ExtractAs<Column>(std::get<Column>(tuple), statement);
		}

		template <size_t Column = 0>
		static inline void ExtractAs(unsigned char* value, sqlite3_stmt* statement)
		{
			assert(sqlite3_column_type(statement, Column) != SQLITE_NULL);
			memcpy(value,
				sqlite3_column_blob(statement, Column),
				sqlite3_column_bytes(statement, Column));
		}
	};

	template <typename Lazy>
	struct SQLiteStatementColumn<const void*, Lazy>
	{
		template <typename Tuple, size_t Column = 0>
		static inline int Bind(Tuple& tuple, sqlite3_stmt* statement)
		{
			return BindAs(std::get<Column>(tuple), Column, statement);
		}

		static inline int BindAs(const void* value, size_t Column, sqlite3_stmt* statement)
		{
			return SQLiteStatementColumn<const unsigned char*>::BindAs((const char*)value, Column, statement);
		}
	};

	template <typename Lazy>
	struct SQLiteStatementColumn<void*, Lazy>
		:
		public SQLiteStatementColumn<const void*>
	{
		template <typename Tuple, size_t Column = 0>
		static inline void Extract(Tuple& tuple, sqlite3_stmt* statement)
		{
			ExtractAs<Column>(std::get<Column>(tuple), statement);
		}

		template <size_t Column = 0>
		static inline void ExtractAs(void* value, sqlite3_stmt* statement)
		{
			SQLiteStatementColumn<unsigned char*>::ExtractAs((unsigned char*)value, statement);
		}
	};

	template <typename Character>
	struct SQLiteStatementColumn<std::basic_string<Character>, std::enable_if_t<!std::is_same_v<Character, char>>>
	{
		typedef std::basic_string<Character> String;

		template <typename Tuple, size_t Column = 0>
		static inline void Extract(Tuple& tuple, sqlite3_stmt* statement)
		{
			ExtractAs<Column>(std::get<Column>(tuple), statement);
		}

		template <size_t Column = 0>
		static inline void ExtractAs(String& value, sqlite3_stmt* statement)
		{
			assert(sqlite3_column_type(statement, Column) != SQLITE_NULL);
			value.assign((const typename String::value_type*) sqlite3_column_blob(statement, Column));
		}

		template <typename Tuple, size_t Column = 0>
		static inline int Bind(Tuple& tuple, sqlite3_stmt* statement)
		{
			return BindAs(std::get<Column>(tuple), Column, statement);
		}

		static inline int BindAs(String& value, size_t Column, sqlite3_stmt* statement)
		{
			return sqlite3_bind_blob(
				statement, Column + 1,
				(const void*)value.c_str(),
				value.size() * sizeof(typename String::value_type),
				SQLITE_TRANSIENT);
		}
	};

	template <typename Lazy>
	struct SQLiteStatementColumn<const wchar_t*, Lazy>
	{
		template <typename Tuple, size_t Column = 0>
		static inline int Bind(Tuple& tuple, sqlite3_stmt* statement)
		{
			return BindAs(std::get<Column>(tuple), Column, statement);
		}

		static inline int BindAs(const wchar_t* value, size_t Column, sqlite3_stmt* statement)
		{
			return sqlite3_bind_blob(
				statement,
				Column + 1, (const void*)value,
				wcslen(value) * sizeof(wchar_t),
				SQLITE_TRANSIENT);
		}
	};

	template <typename Lazy>
	struct SQLiteStatementColumn<wchar_t*, Lazy>
		:
		public SQLiteStatementColumn<const wchar_t*>
	{
		template <typename Tuple, size_t Column = 0>
		static inline void Extract(Tuple& tuple, sqlite3_stmt* statement)
		{
			ExtractAs<Column>(std::get<Column>(tuple), statement);
		}

		template <size_t Column = 0>
		static inline void ExtractAs(wchar_t* value, sqlite3_stmt* statement)
		{
			assert(sqlite3_column_type(statement, Column) != SQLITE_NULL);
			memcpy((void*)value,
				sqlite3_column_blob(statement, Column),
				sqlite3_column_bytes(statement, Column));
		}
	};

	template <typename T, typename... Args>
	struct SQLiteStatementColumnUnpacker
	{
		template <typename Tuple, size_t Column = 0>
		static inline void Extract(Tuple& tuple, sqlite3_stmt* statement)
		{
			SQLiteStatementColumnUnpacker<T>::template Extract<Tuple, Column>(tuple, statement);
			SQLiteStatementColumnUnpacker<Args...>::template Extract<Tuple, Column + 1>(tuple, statement);
		}

		template <typename Tuple, size_t Column = 0>
		static inline int Bind(Tuple& tuple, sqlite3_stmt* statement)
		{
			int result = SQLiteStatementColumnUnpacker<T>::template Bind<Tuple, Column>(tuple, statement);
			if (result != SQLITE_OK)
			{
				return result;
			}

			return SQLiteStatementColumnUnpacker<Args...>::template Bind<Tuple, Column + 1>(tuple, statement);
		}
	};

	template <typename T>
	struct SQLiteStatementColumnUnpacker<T>
	{
		template <typename Tuple, size_t Column = 0>
		static inline void Extract(Tuple& tuple, sqlite3_stmt* statement)
		{
			SQLiteStatementColumn<T>::template Extract<Tuple, Column>(tuple, statement);
		}

		template <typename Tuple, size_t Column = 0>
		static inline int Bind(Tuple& tuple, sqlite3_stmt* statement)
		{
			return SQLiteStatementColumn<T>::template Bind<Tuple, Column>(tuple, statement);
		}
	};

	template <typename Tuple>
	struct SQLiteStatementColumnTuple
	{
	};

	template <typename... Args>
	struct SQLiteStatementColumnTuple<std::tuple<Args...>>
		:
		public SQLiteStatementColumnUnpacker<Args...>
	{
	};

	enum class SQLiteStatementStatus
	{
		Ready,
		Running,
		Finished,
		Failed
	};

	template <typename T>
	class SQLiteStatementIterator
	{
		template <typename...>
		friend class SQLiteStatement;

		enum class Type
		{
			Begin,
			End
		};

		SQLiteStatementIterator(T* statement, Type type)
			:
			statement(statement),
			type(type)
		{
		}

	public:
		typedef typename T::Tuple Tuple;

		typedef Tuple value_type;
		typedef std::ptrdiff_t difference_type;
		typedef Tuple* pointer;
		typedef Tuple& reference;
		typedef std::input_iterator_tag iterator_category;

		const Tuple& operator*() const
		{
			return statement->get_tuple();
		}

		bool operator==(const SQLiteStatementIterator& other) const
		{
			if (statement != other.statement)
				return false;

			return statement->get_status() == SQLiteStatementStatus::Running
				? type == other.type
				: true;
		}

		bool operator!=(const SQLiteStatementIterator& other) const
		{
			return !(*this == other);
		}

		SQLiteStatementIterator& operator++()
		{
			assert(statement->get_status() == SQLiteStatementStatus::Running);
			statement->step();
			return *this;
		}

	private:
		T* statement;
		Type type;
	};

	template <typename... Args>
	class SQLiteStatement
		:
		public SQLiteStatement<std::tuple<Args...>>
	{
		typedef SQLiteStatement<std::tuple<Args...>> Parent;

	public:
		using Parent::Parent;
	};

	template <typename... Args>
	class SQLiteStatement<std::tuple<Args...>>
	{
	public:
		typedef std::tuple<Args...> Tuple;
		typedef SQLiteStatementIterator<SQLiteStatement<Tuple>> Iterator;

		template <typename... Args>
		SQLiteStatement(SQLiteDatabase* database, std::string query, Args... bind_args)
			:
			SQLiteStatement(database, query, std::make_tuple(bind_args...))
		{
		}

		template <typename... Args>
		SQLiteStatement(SQLiteDatabase* database, std::string query, std::tuple<Args...> bind_tuple)
			:
			SQLiteStatement(database, query)
		{
			bind(bind_tuple);
		}

		template <typename... Args>
		SQLiteStatement(SQLiteDatabase* database, std::string query)
			:
			database(database)
		{
			ensureStatusCode(
				sqlite3_prepare_v2(
					database->get_database(),
					query.c_str(),
					-1, &statement, NULL),
				"failed to prepare statement");
		}

		virtual ~SQLiteStatement()
		{
			// finalize only fails if previous functions failed and
			// does not give meaningfull information
			// null check not needed. with null is a noop
			sqlite3_finalize(statement);
		}

		operator bool()
		{
			return status != SQLiteStatementStatus::Failed;
		}

		Iterator begin()
		{
			if (status == SQLiteStatementStatus::Ready)
			{
				step();
			}

			return Iterator{ this,
				status == SQLiteStatementStatus::Running
				? Iterator::Type::Begin
				: Iterator::Type::End };
		}

		Iterator end()
		{
			return Iterator{ this, Iterator::Type::End };
		}

		template <typename T>
		bool execute(T& value)
		{
			static_assert(std::is_same_v<std::tuple<T>, Tuple>,
				"got invalid type in databasecore statement execute<T>");

			return execute<Tuple>(std::make_tuple(value));
		}

		template <>
		bool execute<Tuple>(Tuple& tuple)
		{
			return step(tuple) && execute();
		}

		bool execute()
		{
			switch (status)
			{
			case SQLiteStatementStatus::Failed:
				if (OnDatabaseCoreFailure)
					OnDatabaseCoreFailure("tried to execute statement after failure");

				return false;
			case SQLiteStatementStatus::Finished:
				if (OnDatabaseCoreFailure)
					OnDatabaseCoreFailure("tried to execute statement after finish");

				return false;
			}

			if (int result = sqlite3_step(statement); result != SQLITE_DONE)
			{
				if (result == SQLITE_ROW)
				{
					if (OnDatabaseCoreFailure)
						OnDatabaseCoreFailure("got row in databasecore statement execute<>");
				}
				else
				{
					ensureStatusCode(result, "failed to step statement");
				}

				return false;
			}

			status = SQLiteStatementStatus::Finished;
			return true;
		}

		bool step()
		{
			return step(tuple);
		}

		bool can_step() const
		{
			return status == SQLiteStatementStatus::Running
				|| status == SQLiteStatementStatus::Ready;
		}

		template <typename T>
		bool bind(int index, T& value)
		{
			if (status != SQLiteStatementStatus::Ready)
			{
				if (OnDatabaseCoreFailure)
					OnDatabaseCoreFailure("tried to bind index value in databasecore statement at invalid status");

				return false;
			}

			return ensureStatusCode(
				SQLiteStatementColumn<T>::BindAs(value, index, statement),
				L"failed to bind indexed value");
		}

		template <typename... Args>
		bool bind(Args&&... args)
		{
			std::tuple<Args...> tuple = std::make_tuple<Args...>(std::forward(args)...);
			return bind(tuple);
		}

		template <typename... Args>
		bool bind(std::tuple<Args...>& tuple)
		{
			if (status != SQLiteStatementStatus::Ready)
			{
				if (OnDatabaseCoreFailure)
					OnDatabaseCoreFailure("tried to bind tuple in databasecore statement at invalid status");

				return false;
			}

			return ensureStatusCode(
				SQLiteStatementColumnTuple<std::tuple<Args...>>::template Bind(tuple, statement),
				"failed to bind tuple values");
		}

		SQLiteStatementStatus get_status() const
		{
			return status;
		}

		const Tuple& get_tuple() const
		{
			return tuple;
		}

		sqlite3_stmt* get_statement() const
		{
			return statement;
		}

	private:
		SQLiteDatabase* database;

		SQLiteStatementStatus status;
		Tuple tuple;
		sqlite3_stmt* statement;

		bool step(Tuple& tuple)
		{
			switch (status)
			{
			case SQLiteStatementStatus::Failed:
				if (OnDatabaseCoreFailure)
					OnDatabaseCoreFailure("tried to step statement after failure");

				return false;
			case SQLiteStatementStatus::Finished:
				if (OnDatabaseCoreFailure)
					OnDatabaseCoreFailure("tried to step statement after finish");

				return false;
			}

			if (int result = sqlite3_step(statement); result != SQLITE_ROW)
			{
				if (result == SQLITE_DONE)
				{
					status = SQLiteStatementStatus::Finished;
				}
				else
				{
					ensureStatusCode(result, "failed to step statement");
				}

				return false;
			}

			if (status == SQLiteStatementStatus::Ready)
				status = SQLiteStatementStatus::Running;

			SQLiteStatementColumnTuple<Tuple>::Extract(tuple, statement);
			return true;
		}

		bool ensureStatusCode(int code, const char* message)
		{
			if (!EnsureSQLiteStatusCode(code, message))
			{
				if (statement)
				{
					EnsureSQLiteStatusCode(
						sqlite3_finalize(statement),
						"failed to finalize after failure");
					statement = NULL;
				}

				status = SQLiteStatementStatus::Failed;
				return false;
			}

			return true;
		}
	};

	template <typename... Args>
	using Statement = SQLiteStatement<Args...>;
	using Database = SQLiteDatabase;

	static_assert(sizeof(char) == 1, "DatabaseCore is written for character size 1");
}

#pragma warning(pop)

using Database::SQLiteInt;
using Database::SQLiteReal;
using Database::SQLiteString;
using Database::SQLiteBlob;
