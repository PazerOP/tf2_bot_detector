#pragma once

#ifndef DB_HELPERS_OK
#error DBHelpers.h leaked into an external header
#endif

#include <SQLiteCpp/SQLiteCpp.h>
#include <mh/types/enum_class_bit_ops.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <variant>

namespace SQLite
{
	class Database;
}

namespace tf2_bot_detector::DB
{
	struct ColumnDefinition;

	enum class ColumnType
	{
		Integer,
		Float,
		Text,
		Blob,
	};

	enum class BinaryOperator
	{
		Equals,
		NotEquals,
		GreaterThan,
		LessThan,
		GreaterThanOrEqual,
		LessThanOrEqual,
		//BitwiseAnd,
		//BitwiseOr,
	};

	enum class CreateTableFlags
	{
		None = 0,
		IfNotExists = (1 << 0),
	};
	MH_ENABLE_ENUM_CLASS_BIT_OPS(CreateTableFlags);

	enum class ColumnFlags
	{
		None = 0,
		PrimaryKey = (1 << 0),
		NotNull = (1 << 1),
		Unique = (1 << 2),

		PrimaryKeyDefaults = PrimaryKey | NotNull | Unique,
	};
	MH_ENABLE_ENUM_CLASS_BIT_OPS(ColumnFlags);

	class IOperationExpression
	{
	public:
		virtual ~IOperationExpression() = default;
	};

	struct BlobData
	{
		const void* m_Data;
		size_t m_Size;
	};

	using DBData_t = std::variant<int64_t, double, const char*, BlobData>;
	class ConstantExpression final : public IOperationExpression
	{
	public:
		DBData_t m_Data;
	};

	class ColumnExpression final : public IOperationExpression
	{
	public:
		std::reference_wrapper<const ColumnDefinition> m_Column;
	};

	struct BinaryOperation
	{
		BinaryOperation(BinaryOperator operation, std::unique_ptr<IOperationExpression> lhs, std::unique_ptr<IOperationExpression> rhs);

		std::unique_ptr<IOperationExpression> m_LHS;
		std::unique_ptr<IOperationExpression> m_RHS;
		BinaryOperator m_Operation;
	};

	struct ColumnDefinition
	{
		constexpr ColumnDefinition(const char* name, ColumnType type, ColumnFlags flags = ColumnFlags::None) :
			m_Name(name), m_Type(type), m_Flags(flags)
		{
		}

		//constexpr operator const char* () const { return m_Name; }

		const char* m_Name = nullptr;
		ColumnType m_Type{};
		ColumnFlags m_Flags{};
	};

	struct ColumnData
	{
		ColumnData(const ColumnDefinition& column, uint32_t intData);
		ColumnData(const ColumnDefinition& column, int64_t intData);
		ColumnData(const ColumnDefinition& column, double realData);
		ColumnData(const ColumnDefinition& column, const char* textData);
		ColumnData(const ColumnDefinition& column, const BlobData& blobData);

		std::reference_wrapper<const ColumnDefinition> m_Column;
		DBData_t m_Data;
	};

	void CreateTable(SQLite::Database& db, std::string tableName, std::initializer_list<ColumnDefinition> columns,
		CreateTableFlags flags = CreateTableFlags::None);

	enum class InsertIntoConstraintResolver
	{
		Abort,
		Fail,
		Ignore,
		Replace,
		Rollback,
	};

	void InsertInto(SQLite::Database& db, const std::string_view& tableName, std::initializer_list<ColumnData> columns,
		InsertIntoConstraintResolver resolver = InsertIntoConstraintResolver::Abort);
	void ReplaceInto(SQLite::Database& db, const std::string_view& tableName, std::initializer_list<ColumnData> columns);
	//void SelectAllColumns(SQLite::Database& db, const std::string_view& tableName)
}
