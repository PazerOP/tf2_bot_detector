#define DB_HELPERS_OK 1

#include "DBHelpers.h"

#include <mh/error/ensure.hpp>
#include <SQLiteCpp/SQLiteCpp.h>

void tf2_bot_detector::DB::CreateTable(SQLite::Database& db, std::string tableName,
	std::initializer_list<ColumnDefinition> columns, CreateTableFlags flags) try
{
	std::string query = "CREATE TABLE";

	if (flags & CreateTableFlags::IfNotExists)
		query += " IF NOT EXISTS";

	query.append(" \"").append(tableName).append("\" (");

	assert(columns.size() > 0);

	const ColumnDefinition* primaryKey = nullptr;
	for (const ColumnDefinition& column : columns)
	{
		if (&column != columns.begin())
			query.append(","); // Not the first column, add a comma

		query.append("\n\t\"").append(column.m_Name).append("\"");

		switch (column.m_Type)
		{
		case ColumnType::Blob:
			query.append(" BLOB");
			break;
		case ColumnType::Float:
			query.append(" REAL");
			break;
		case ColumnType::Integer:
			query.append(" INTEGER");
			break;
		case ColumnType::Text:
			query.append(" TEXT");
			break;
		}

		if (column.m_Flags & ColumnFlags::NotNull)
			query.append(" NOT NULL");
		if (column.m_Flags & ColumnFlags::Unique)
			query.append(" UNIQUE");

		if ((column.m_Flags & ColumnFlags::PrimaryKey) && mh_ensure(!primaryKey))
			primaryKey = &column;
	}

	if (primaryKey)
		query.append(",\n\tPRIMARY KEY(\"").append(primaryKey->m_Name).append("\")");

	query.append("\n);");

	db.exec(query);
}
catch (...)
{
	LogException();
	throw;
}

void tf2_bot_detector::DB::InsertInto(SQLite::Database& db, const std::string_view& tableName, std::initializer_list<ColumnData> columns,
	InsertIntoConstraintResolver resolver) try
{
	std::string query = "INSERT OR ";

	switch (resolver)
	{
	case InsertIntoConstraintResolver::Abort:
		query += "ABORT";
		break;
	case InsertIntoConstraintResolver::Fail:
		query += "FAIL";
		break;
	case InsertIntoConstraintResolver::Ignore:
		query += "IGNORE";
		break;
	case InsertIntoConstraintResolver::Replace:
		query += "REPLACE";
		break;
	case InsertIntoConstraintResolver::Rollback:
		query += "ROLLBACK";
		break;
	}

	query.append(" INTO \"").append(tableName).append("\" (");

	for (const ColumnData& column : columns)
	{
		if (&column != columns.begin())
			query.append(", ");

		query.append("\"").append(column.m_Column.get().m_Name).append("\"");
	}

	query.append(") VALUES (");

	{
		int i = 0;
		for (const ColumnData& column : columns)
		{
			if (&column != columns.begin())
				query.append(", ");

			mh::format_to(std::back_inserter(query), "?{}", ++i);
		}
	}

	query.append(")");

	SQLite::Statement statement(db, query);

	{
		int i = 1;
		for (const ColumnData& column : columns)
		{
			std::visit([&](const auto& val)
				{
					using type = std::decay_t<decltype(val)>;
					if constexpr (std::is_same_v<type, BlobData>)
						statement.bind(i, val.m_Data, static_cast<int>(val.m_Size));
					else
						statement.bind(i, val);

				}, column.m_Data);

			i++;
		}
	}

	statement.exec();
}
catch (...)
{
	LogException();
	throw;
}

void tf2_bot_detector::DB::ReplaceInto(SQLite::Database& db, const std::string_view& tableName, std::initializer_list<ColumnData> columns)
{
	return InsertInto(db, tableName, columns, InsertIntoConstraintResolver::Replace);
}

tf2_bot_detector::DB::ColumnData::ColumnData(const ColumnDefinition& column, uint32_t intData) :
	m_Column(column), m_Data(static_cast<int64_t>(intData))
{
	assert(column.m_Type == ColumnType::Integer);
}

tf2_bot_detector::DB::ColumnData::ColumnData(const ColumnDefinition& column, int64_t intData) :
	m_Column(column), m_Data(intData)
{
	assert(column.m_Type == ColumnType::Integer);
}

tf2_bot_detector::DB::ColumnData::ColumnData(const ColumnDefinition& column, double realData) :
	m_Column(column), m_Data(realData)
{
	assert(column.m_Type == ColumnType::Float);
}

tf2_bot_detector::DB::ColumnData::ColumnData(const ColumnDefinition& column, const char* textData) :
	m_Column(column), m_Data(textData)
{
	assert(column.m_Type == ColumnType::Text);
}

tf2_bot_detector::DB::ColumnData::ColumnData(const ColumnDefinition& column, const BlobData& blobData) :
	m_Column(column), m_Data(blobData)
{
	assert(column.m_Type == ColumnType::Blob);
}

tf2_bot_detector::DB::BinaryOperation::BinaryOperation(BinaryOperator operation,
	std::unique_ptr<IOperationExpression> lhs, std::unique_ptr<IOperationExpression> rhs) :
	m_LHS(std::move(lhs)), m_RHS(std::move(rhs)), m_Operation(operation)
{
}
