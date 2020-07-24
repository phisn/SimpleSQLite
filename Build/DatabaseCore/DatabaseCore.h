#pragma once

#include "sqlite3.h"

#include <cassert>
#include <functional>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

#pragma comment(lib, "sqlite.lib")

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
	};

	template <typename T, typename Lazy = void>
	struct StatementColumn
	{
	};

	template <typename T, typename Lazy>
	struct StatementColumn<std::optional<T>, Lazy>
	{
		template <typename Tuple, int Column = 0>
		static inline void Extract(Tuple& tuple, sqlite3_stmt* statement)
		{
			if (sqlite3_column_type(statement, Column) == SQLITE_NULL)
			{
				std::get<Column>(tuple).reset();
			}
			else
			{
				StatementColumn<T>::template ExtractAs<Tuple, Column>(
					*std::get<Column>(tuple), statement);
			}
		}

		template <typename Tuple, int Column = 0>
		static inline int Bind(Tuple& tuple, sqlite3_stmt* statement)
		{
			if (std::get<Column>(tuple))
			{
				return StatementColumn<T>::template BindAs(
					*std::get<Column>(tuple), Column, statement);
			}
			else
			{
				return sqlite3_bind_null(statement, Column + 1);
			}
		}
	};

	template <typename Lazy>
	struct StatementColumn<std::nullopt_t, Lazy>
	{
		template <typename TupleType, int Column = 0>
		static inline int Bind(TupleType& tuple, sqlite3_stmt* statement)
		{
			return sqlite3_bind_null(statement, Column + 1);
		}
	};

	template <typename T>
	struct StatementColumn<T, std::enable_if_t<std::is_integral_v<T>>>
	{
		template <typename Tuple, int Column = 0>
		static inline void Extract(Tuple& tuple, sqlite3_stmt* statement)
		{
			ExtractAs<Column>((SQLiteInt&) std::get<Column>(tuple), statement);
		}

		template <int Column = 0>
		static inline void ExtractAs(SQLiteInt& value, sqlite3_stmt* statement)
		{
			assert(sqlite3_column_type(statement, Column) != SQLITE_NULL);
			value = sqlite3_column_int64(statement, Column);
		}

		template <typename Tuple, int Column = 0>
		static inline int Bind(Tuple& tuple, sqlite3_stmt* statement)
		{
			return BindAs((SQLiteInt) std::get<Column>(tuple), Column, statement);
		}

		static inline int BindAs(SQLiteInt value, int Column, sqlite3_stmt* statement)
		{
			return sqlite3_bind_int64(statement, Column + 1, value);
		}
	};

	template <typename T>
	struct StatementColumn<T, std::enable_if_t<std::is_floating_point_v<T>>>
	{
		template <typename Tuple, int Column = 0>
		static inline void Extract(Tuple& tuple, sqlite3_stmt* statement)
		{
			ExtractAs<Column>((SQLiteReal&) std::get<Column>(tuple), statement);
		}

		template <int Column = 0>
		static inline void ExtractAs(SQLiteReal& value, sqlite3_stmt* statement)
		{
			assert(sqlite3_column_type(statement, Column) != SQLITE_NULL);
			value = sqlite3_column_double(statement, Column);
		}

		template <typename Tuple, int Column = 0>
		static inline int Bind(Tuple& tuple, sqlite3_stmt* statement)
		{
			return BindAs((SQLiteReal) std::get<Column>(tuple), Column, statement);
		}

		static inline int BindAs(SQLiteReal value, int Column, sqlite3_stmt* statement)
		{
			return sqlite3_bind_double(statement, Column + 1, value);
		}
	};

	template <typename Lazy>
	struct StatementColumn<SQLiteString, Lazy>
	{
		template <typename Tuple, int Column = 0>
		static inline void Extract(Tuple& tuple, sqlite3_stmt* statement)
		{
			ExtractAs<Column>(std::get<Column>(tuple), statement);
		}

		template <int Column = 0>
		static inline void ExtractAs(SQLiteString& value, sqlite3_stmt* statement)
		{
			assert(sqlite3_column_type(statement, Column) != SQLITE_NULL);
			value.assign((const char*) sqlite3_column_text(statement, Column));
		}

		template <typename Tuple, int Column = 0>
		static inline int Bind(Tuple& tuple, sqlite3_stmt* statement)
		{
			return BindAs(std::get<Column>(tuple), Column, statement);
		}

		static inline int BindAs(SQLiteString& value, int Column, sqlite3_stmt* statement)
		{
			return sqlite3_bind_text(
				statement, Column + 1, value.c_str(),
				value.size(),
				SQLITE_TRANSIENT);
		}
	};

	template <typename Lazy>
	struct StatementColumn<const char*, Lazy>
	{
		template <typename Tuple, int Column = 0>
		static inline int Bind(Tuple& tuple, sqlite3_stmt* statement)
		{
			return BindAs(std::get<Column>(tuple), Column, statement);
		}

		static inline int BindAs(const char* value, int Column, sqlite3_stmt* statement)
		{
			return sqlite3_bind_text(
				statement,
				Column + 1, value,
				-1,
				SQLITE_TRANSIENT);
		}
	};

	template <typename Lazy>
	struct StatementColumn<char*, Lazy>
		:
		public StatementColumn<const char*>
	{
		template <typename Tuple, int Column = 0>
		static inline void Extract(Tuple& tuple, sqlite3_stmt* statement)
		{
			ExtractAs<Column>(std::get<Column>(tuple), statement);
		}

		template <int Column = 0>
		static inline void ExtractAs(char* value, sqlite3_stmt* statement)
		{
			assert(sqlite3_column_type(statement, Column) != SQLITE_NULL);
			memcpy(value, 
				(const void*) sqlite3_column_text(statement, Column), 
				sqlite3_column_bytes(statement, Column));
		}
	};

	template <typename Lazy>
	struct StatementColumn<SQLiteBlob, Lazy>
	{
		template <typename Tuple, int Column = 0>
		static inline void Extract(Tuple& tuple, sqlite3_stmt* statement)
		{
			ExtractAs<Column>(std::get<Column>(tuple), statement);
		}

		template <int Column = 0>
		static inline void ExtractAs(SQLiteBlob& value, sqlite3_stmt* statement)
		{
			assert(sqlite3_column_type(statement, Column) != SQLITE_NULL);
			const char* data = (const char*) sqlite3_column_blob(statement, Column);
			value.assign(data, data + sqlite3_column_bytes(statement, Column));
		}

		template <typename TupleType, int Column = 0>
		static inline int Bind(TupleType& tuple, sqlite3_stmt* statement)
		{
			return BindAs(std::get<Column>(tuple), Column, statement);
		}

		static inline int BindAs(SQLiteBlob& value, int Column, sqlite3_stmt* statement)
		{
			return sqlite3_bind_blob(
				statement, Column + 1,
				(const void*) &value[0],
				value.size(),
				SQLITE_TRANSIENT);
		}
	};

	template <typename Lazy>
	struct StatementColumn<const unsigned char*, Lazy>
	{
		template <typename Tuple, int Column = 0>
		static inline int Bind(Tuple& tuple, sqlite3_stmt* statement)
		{
			return BindAs(std::get<Column>(tuple), Column, statement);
		}

		static inline int BindAs(const unsigned char* value, int Column, sqlite3_stmt* statement)
		{
			return sqlite3_bind_blob(
				statement,
				Column + 1, (const void*)value,
				strlen((const char*)value),
				SQLITE_TRANSIENT);
		}
	};

	template <typename Lazy>
	struct StatementColumn<unsigned char*, Lazy>
		:
		public StatementColumn<const unsigned char*>
	{
		template <typename Tuple, int Column = 0>
		static inline void Extract(Tuple& tuple, sqlite3_stmt* statement)
		{
			ExtractAs<Column>(std::get<Column>(tuple), statement);
		}

		template <int Column = 0>
		static inline void ExtractAs(unsigned char* value, sqlite3_stmt* statement)
		{
			assert(sqlite3_column_type(statement, Column) != SQLITE_NULL);
			memcpy(value, 
				sqlite3_column_blob(statement, Column), 
				sqlite3_column_bytes(statement, Column));
		}
	};

	template <typename Lazy>
	struct StatementColumn<const void*, Lazy>
	{
		template <typename Tuple, int Column = 0>
		static inline int Bind(Tuple& tuple, sqlite3_stmt* statement)
		{
			return BindAs(std::get<Column>(tuple), Column, statement);
		}

		static inline int BindAs(const void* value, int Column, sqlite3_stmt* statement)
		{
			return StatementColumn<const unsigned char*>::BindAs((const char*)value, Column, statement);
		}
	};

	template <typename Lazy>
	struct StatementColumn<void*, Lazy>
		:
		public StatementColumn<const void*>
	{
		template <typename Tuple, int Column = 0>
		static inline void Extract(Tuple& tuple, sqlite3_stmt* statement)
		{
			ExtractAs<Column>(std::get<Column>(tuple), statement);
		}

		template <int Column = 0>
		static inline void ExtractAs(void* value, sqlite3_stmt* statement)
		{
			StatementColumn<unsigned char*>::ExtractAs((unsigned char*) value, statement);
		}
	};

	template <typename Character>
	struct StatementColumn<std::basic_string<Character>, std::enable_if_t<!std::is_same_v<Character, char>>>
	{
		typedef std::basic_string<Character> String;

		template <typename Tuple, int Column = 0>
		static inline void Extract(Tuple& tuple, sqlite3_stmt* statement)
		{
			ExtractAs<Column>(std::get<Column>(tuple), statement);
		}

		template <int Column = 0>
		static inline void ExtractAs(String& value, sqlite3_stmt* statement)
		{
			assert(sqlite3_column_type(statement, Column) != SQLITE_NULL);
			value.assign((const typename String::value_type*) sqlite3_column_blob(statement, Column));
		}

		template <typename Tuple, int Column = 0>
		static inline int Bind(Tuple& tuple, sqlite3_stmt* statement)
		{
			return BindAs(std::get<Column>(tuple), Column, statement);
		}

		static inline int BindAs(String& value, int Column, sqlite3_stmt* statement)
		{
			return sqlite3_bind_blob(
				statement, Column + 1,
				(const void*) value.c_str(),
				value.size() * sizeof(typename String::value_type),
				SQLITE_TRANSIENT);
		}
	};

	template <typename Lazy>
	struct StatementColumn<const wchar_t*, Lazy>
	{
		template <typename Tuple, int Column = 0>
		static inline int Bind(Tuple& tuple, sqlite3_stmt* statement)
		{
			return BindAs(std::get<Column>(tuple), Column, statement);
		}

		static inline int BindAs(const wchar_t* value, int Column, sqlite3_stmt* statement)
		{
			return sqlite3_bind_blob(
				statement,
				Column + 1, (const void*) value,
				wcslen(value) * sizeof(wchar_t),
				SQLITE_TRANSIENT);
		}
	};

	template <typename Lazy>
	struct StatementColumn<wchar_t*, Lazy>
		:
		public StatementColumn<const wchar_t*>
	{
		template <typename Tuple, int Column = 0>
		static inline void Extract(Tuple& tuple, sqlite3_stmt* statement)
		{
			ExtractAs<Column>(std::get<Column>(tuple), statement);
		}

		template <int Column = 0>
		static inline void ExtractAs(wchar_t* value, sqlite3_stmt* statement)
		{
			assert(sqlite3_column_type(statement, Column) != SQLITE_NULL);
			memcpy((void*) value,
				sqlite3_column_blob(statement, Column),
				sqlite3_column_bytes(statement, Column));
		}
	};

	template <typename T, typename... Args>
	struct StatementColumnUnpacker
	{
		template <typename Tuple, int Column = 0>
		static inline void Extract(Tuple& tuple, sqlite3_stmt* statement)
		{
			StatementColumnUnpacker<T>::template Extract<Tuple, Column>(tuple, statement);
			StatementColumnUnpacker<Args...>::template Extract<Tuple, Column + 1>(tuple, statement);
		}

		template <typename Tuple, int Column = 0>
		static inline int Bind(TupleType& tuple, sqlite3_stmt* statement)
		{
			int result = StatementColumnUnpacker<Tuple>::template Bind<TupleType, Column>(tuple, statement);
			if (result != SQLITE_OK)
			{
				return result;
			}

			return StatementColumnUnpacker<Args...>::template Bind<Tuple, Column + 1>(tuple, statement);
		}
	};

	template <typename T>
	struct StatementColumnUnpacker
	{
		template <typename Tuple, int Column = 0>
		static inline void Extract(Tuple& tuple, sqlite3_stmt* statement)
		{
			StatementColumn<T>::template Extract<Tuple, Column>(tuple, statement);
		}

		template <typename Tuple, int Column = 0>
		static inline int Bind(Tuple& tuple, sqlite3_stmt* statement)
		{
			return StatementColumn<T>::template Bind<Tuple, Column>(tuple, statement);
		}
	};

	template <typename Tuple>
	struct StatementColumnTuple
	{
	};

	template <typename... Args>
	struct StatementColumnTuple<std::tuple<Args...>>
		:
		public StatementColumnUnpacker<Args...>
	{
	};

	enum class StatementStatus
	{
		Ready,
		Running,
		Finished,
		Failed
	};

	template <typename T>
	class StatementIterator
	{
		template <typename>
		friend class Statement;

		enum class Type
		{
			Begin,
			End
		};

		StatementIterator(T* statement, Type type)
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

		bool operator==(const StatementIterator& other) const
		{
			if (statement != other.statement)
				return false;

			return statement->get_status() == StatementStatus::Running
				? type == other.type
				: true;
		}

		bool operator!=(const StatementIterator& other) const
		{
			return !(*this == other);
		}

		StatementIterator& operator++()
		{
			assert(statement->get_status() == StatementStatus::Running);
			statement->step();
			return *this;
		}

	private:
		T* statement;
		Type type;
	};

	template <typename Tuple = std::tuple<>>
	class Statement
	{
	public:
		typedef Tuple Tuple;
		typedef StatementIterator<Statement<Tuple>> Iterator;

		template <typename... Args>
		Statement(SQLiteDatabase* database, std::string query, Args&&... bind_args)
			:
			Statement(database, query, std::make_tuple(std::forward(bind_args)...))
		{
		}

		template <typename... Args>
		Statement(SQLiteDatabase* database, std::string query, std::tuple<Args...> bind_tuple)
			:
			Statement(database, query)
		{
			bind(bind_tuple);
		}

		template <typename... Args>
		Statement(SQLiteDatabase* database, std::string query)
			:
			database(database)
		{
			ensureStatusCode(
				sqlite3_prepare_v2(
					database->raw(),
					query.c_str(),
					-1, &statement, NULL), 
				L"failed to prepare statement");
		}

		virtual ~Statement()
		{
			// finalize only fails if previous functions failed and
			// does not give meaningfull information
			// null check not needed. with null is a noop
			ensureStatusCode(
				sqlite3_finalize(statement),
				"failed to finalize");
		}

		operator bool()
		{
			return status != StatementStatus::Failed;
		}

		Iterator begin()
		{
			if (status == StatementStatus::Ready)
			{
				step();
			}
			
			return Iterator{ this, 
				status == StatementStatus::Running
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
			return step() && execute();
		}

		bool execute()
		{
			switch (status)
			{
			case StatementStatus::Failed:
				OnDatabaseCoreFailure("tried to execute statement after failure");

				return false;
			case StatementStatus::Finished:
				OnDatabaseCoreFailure("tried to execute statement after finish");

				return false;
			}

			if (int result = sqlite3_step(statement); result != SQLITE_DONE)
			{
				if (result == SQLITE_ROW)
				{
					OnDatabaseCoreFailure("got row in databasecore statement execute<>");
				}
				else
				{
					ensureStatusCode(result, "failed to step statement");
				}

				return false;
			}

			status = StatementStatus::Finished;
			return true;
		}

		bool step()
		{
			return step(tuple);
		}

		bool can_step() const
		{
			return status == StatementStatus::Running;
		}

		template <typename T>
		bool bind(int index, T& value)
		{
			if (status != StatementStatus::Ready)
			{
				OnDatabaseCoreFailure("tried to bind index value in databasecore statement at invalid status");
				return false;
			}

			return ensureStatusCode(StatementColumn<T>::BindAs(value, index, statement)
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
			if (status != StatementStatus::Ready)
			{
				OnDatabaseCoreFailure("tried to bind tuple in databasecore statement at invalid status");
				return false;
			}

			return ensureStatusCode(StatementTupleColumn<std::tuple<Args...>>::Bind(tuple, statement);
		}

		StatementStatus get_status() const
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

		StatementStatus status;
		Tuple tuple;
		sqlite3_stmt* statement;

		bool step(Tuple& tuple)
		{
			switch (status)
			{
			case StatementStatus::Failed:
				OnDatabaseCoreFailure("tried to step statement after failure");

				return false;
			case StatementStatus::Finished:
				OnDatabaseCoreFailure("tried to step statement after finish");

				return false;
			}

			if (int result = sqlite3_step(statement); result != SQLITE_ROW)
			{
				if (result == SQLITE_DONE)
				{
					status = StatementStatus::Finished;
				}
				else
				{
					ensureStatusCode(result, "failed to step statement");
				}

				return false;
			}

			StatementColumnTuple<Tuple>::Extract(tuple, statement);
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

				status = StatementStatus::Failed;
				return false;
			}

			return true;
		}
	};

	static_assert(sizeof(char) == 1, "DatabaseCore is written for character size 1");
}

using Database::SQLiteInt;
using Database::SQLiteReal;
using Database::SQLiteString;
using Database::SQLiteBlob;