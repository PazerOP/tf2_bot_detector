#define DB_HELPERS_OK 1

#include "DBHelpers.h"

#include <mh/error/ensure.hpp>
#include <mh/text/fmtstr.hpp>
#include <SQLiteCpp/SQLiteCpp.h>

using namespace tf2_bot_detector;
using namespace tf2_bot_detector::DB;

IStatementGenerator& IStatementGenerator::TextQuoted(const std::string_view& text)
{
	return Text(mh::format("{}", std::quoted(text)));
}

void tf2_bot_detector::DB::CreateTable(SQLite::Database& db, const std::string_view& tableName,
	const ColumnDefinition* columnsBegin, const ColumnDefinition* columnsEnd, CreateTableFlags flags) try
{
	std::string query = "CREATE TABLE";

	if (flags & CreateTableFlags::IfNotExists)
		query += " IF NOT EXISTS";

	query.append(" \"").append(tableName).append("\" (");

	assert((columnsEnd - columnsBegin) > 0);

	const ColumnDefinition* primaryKey = nullptr;
	for (auto columnIt = columnsBegin; columnIt != columnsEnd; columnIt++)
	{
		const ColumnDefinition& column = *columnIt;

		if (&column != columnsBegin)
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

void tf2_bot_detector::DB::CreateTable(SQLite::Database& db, const std::string_view& tableName,
	std::initializer_list<ColumnDefinition> columns, CreateTableFlags flags)
{
	return CreateTable(db, tableName, columns.begin(), columns.end(), flags);
}

void tf2_bot_detector::DB::CreateTable(SQLite::Database& db, const TableDefinition& table, CreateTableFlags flags)
{
	const auto& cols = table.GetColumns();
	return CreateTable(db, table.GetTableName(), cols.data(), cols.data() + cols.size(), flags);
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
					else if constexpr (std::is_same_v<type, std::monostate>)
						statement.bind(i);
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

ColumnData::ColumnData(const ColumnDefinition& column, uint32_t intData) :
	ColumnData(column, int64_t(intData))
{
}

ColumnData::ColumnData(const ColumnDefinition& column, int32_t intData) :
	ColumnData(column, int64_t(intData))
{
}

ColumnData::ColumnData(const ColumnDefinition& column, int64_t intData) :
	m_Column(column), m_Data(intData)
{
	assert(column.m_Type == ColumnType::Integer);
}

ColumnData::ColumnData(const ColumnDefinition& column, double realData) :
	m_Column(column), m_Data(realData)
{
	assert(column.m_Type == ColumnType::Float);
}

ColumnData::ColumnData(const ColumnDefinition& column, const char* textData) :
	m_Column(column), m_Data(textData)
{
	assert(column.m_Type == ColumnType::Text);
}

ColumnData::ColumnData(const ColumnDefinition& column, const BlobData& blobData) :
	m_Column(column), m_Data(blobData)
{
	assert(column.m_Type == ColumnType::Blob);
}

BinaryOperation::BinaryOperation(BinaryOperator operation,
	std::unique_ptr<IOperationExpression> lhs, std::unique_ptr<IOperationExpression> rhs) :
	m_LHS(std::move(lhs)), m_RHS(std::move(rhs)), m_Operation(operation)
{
}

void BinaryOperation::GenerateStatement(IStatementGenerator& gen) const
{
	gen.Text("(");

	m_LHS->GenerateStatement(gen);

	switch (m_Operation)
	{
	case BinaryOperator::Equals:
		gen.Text(" == ");
		break;
	case BinaryOperator::NotEquals:
		gen.Text(" != ");
		break;
	case BinaryOperator::GreaterThan:
		gen.Text(" > ");
		break;
	case BinaryOperator::GreaterThanOrEqual:
		gen.Text(" >= ");
		break;
	case BinaryOperator::LessThan:
		gen.Text(" < ");
		break;
	case BinaryOperator::LessThanOrEqual:
		gen.Text(" <= ");
		break;
	case BinaryOperator::LogicalAnd:
		gen.Text(" && ");
		break;
	case BinaryOperator::LogicalOr:
		gen.Text(" || ");
		break;
	}

	m_RHS->GenerateStatement(gen);

	gen.Text(")");
}

std::unique_ptr<IOperationExpression> BinaryOperation::Clone() const
{
	return std::make_unique<BinaryOperation>(m_Operation, m_LHS->Clone(), m_RHS->Clone());
}

BinaryOperation tf2_bot_detector::DB::operator&&(const BinaryOperation& lhs, const BinaryOperation& rhs)
{
	return BinaryOperation(BinaryOperator::LogicalAnd, lhs.Clone(), rhs.Clone());
}
BinaryOperation tf2_bot_detector::DB::operator||(const BinaryOperation& lhs, const BinaryOperation& rhs)
{
	return BinaryOperation(BinaryOperator::LogicalOr, lhs.Clone(), rhs.Clone());
}

Column2::Column2(SQLite::Column&& innerColumn) :
	SQLite::Column(std::move(innerColumn))
{
}

Statement2::Statement2(SQLite::Statement&& innerStatement) :
	SQLite::Statement(std::move(innerStatement))
{
}

Column2 Statement2::getColumn(const ColumnDefinition& definition)
{
	return SQLite::Statement::getColumn(definition.m_Name);
}

std::unique_ptr<ColumnExpression> tf2_bot_detector::DB::CreateOperationExpression(const ColumnDefinition& column)
{
	return std::make_unique<ColumnExpression>(column);
}

void ConstantExpression::GenerateStatement(IStatementGenerator& gen) const
{
	std::visit([&](const auto& val)
		{
			gen.Param(val);
		}, m_Data);
}

std::unique_ptr<IOperationExpression> ConstantExpression::Clone() const
{
	return std::make_unique<ConstantExpression>(*this);
}

ColumnExpression::ColumnExpression(const ColumnDefinition& column) :
	m_Column(column)
{
}

void ColumnExpression::GenerateStatement(IStatementGenerator& gen) const
{
	gen.TextQuoted(m_Column.m_Name);
}

std::unique_ptr<IOperationExpression> ColumnExpression::Clone() const
{
	return std::make_unique<ColumnExpression>(*this);
}

SelectStatementBuilder::SelectStatementBuilder(const std::string_view& tableName) :
	m_TableName(tableName)
{
}

SelectStatementBuilder& SelectStatementBuilder::Where(BinaryOperation&& binOp)
{
	assert(!m_WhereCondition);
	m_WhereCondition.emplace(std::move(binOp));
	return *this;
}

namespace
{
	struct StringStatementGenerator final : public IStatementGenerator
	{
		StringStatementGenerator(std::string& targetString) : m_TargetString(targetString) {}

		StringStatementGenerator& Text(const std::string_view& text) override
		{
			m_TargetString.append(text);
			return *this;
		}

		StringStatementGenerator& Param(int64_t value) override { return ParamImpl(value); }
		StringStatementGenerator& Param(double value) override { return ParamImpl(value); }
		StringStatementGenerator& Param(std::string value) override { return ParamImpl(std::move(value)); }
		StringStatementGenerator& Param(const BlobData& value) override { return ParamImpl(value); }

		std::string& m_TargetString;
		std::vector<std::variant<int64_t, double, std::string, BlobData>> m_Parameters;

	private:
		template<typename T>
		StringStatementGenerator& ParamImpl(T&& value)
		{
			m_Parameters.push_back(std::move(value));
			return Text(mh::fmtstr<32>("?{}", m_Parameters.size())); // parameter indices start at 1
		}
	};
}

Statement2 SelectStatementBuilder::Run(const SQLite::Database& db) const
{
	std::string queryStr;

	queryStr.append("SELECT * FROM \"").append(m_TableName).append("\"");

	StringStatementGenerator gen(queryStr);
	if (m_WhereCondition)
	{
		queryStr.append(" WHERE ");
		m_WhereCondition->GenerateStatement(gen);
	}

	// The const_cast is "ok" because its just a select statement... right?
	Statement2 retVal(SQLite::Statement(const_cast<SQLite::Database&>(db), queryStr));

	for (size_t i = 0; i < gen.m_Parameters.size(); i++)
	{
		const mh::fmtstr<32> argName("?{}", (i + 1));

		std::visit([&](auto&& arg)
			{
				using type_t = std::decay_t<decltype(arg)>;
				if constexpr (std::is_same_v<type_t, BlobData>)
					retVal.bind(int(i + 1), arg.m_Data, (int)arg.m_Size);
				else if constexpr (std::is_same_v<type_t, std::string>)
					retVal.bind(int(i + 1), arg.c_str());
				else
					retVal.bind(int(i + 1), arg);

			}, gen.m_Parameters[i]);
	}

	return retVal;
}
