#pragma once

#ifndef DB_HELPERS_OK
#error DBHelpers.h leaked into an external header
#endif

#include <SQLiteCpp/SQLiteCpp.h>
#include <mh/types/enum_class_bit_ops.hpp>

#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <optional>
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
		LogicalAnd,
		LogicalOr,
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

	struct BlobData
	{
		const void* m_Data;
		size_t m_Size;
	};

	class IStatementGenerator
	{
	public:
		virtual ~IStatementGenerator() = default;

		virtual IStatementGenerator& Text(const std::string_view& text) = 0;
		virtual IStatementGenerator& TextQuoted(const std::string_view& text);
		virtual IStatementGenerator& Param(int64_t value) = 0;
		virtual IStatementGenerator& Param(double value) = 0;
		virtual IStatementGenerator& Param(std::string value) = 0;
		virtual IStatementGenerator& Param(const BlobData& value) = 0;
	};

	class IOperationExpression
	{
	public:
		virtual ~IOperationExpression() = default;

		virtual void GenerateStatement(IStatementGenerator& gen) const = 0;

		virtual std::unique_ptr<IOperationExpression> Clone() const = 0;
	};

	struct BinaryOperation;
	class ConstantExpression;

	template<typename T>
	concept ConstantExpressionValue =
		std::integral<std::decay_t<T>> ||
		std::floating_point<std::decay_t<T>> ||
		std::same_as<std::decay_t<T>, char*> ||
		std::same_as<std::decay_t<T>, const char*> ||
		std::same_as<std::decay_t<T>, std::string> ||
		std::same_as<std::decay_t<T>, BlobData>;

	template<typename T>
	concept BinaryExpressionArg =
		std::same_as<std::decay_t<T>, BinaryOperation>
		;

	using DBData_t = std::variant<std::monostate, int64_t, double, const char*, BlobData>;
	class ConstantExpression final : public IOperationExpression
	{
	public:
		void GenerateStatement(IStatementGenerator& gen) const override;
		std::unique_ptr<IOperationExpression> Clone() const override;

		std::variant<int64_t, double, std::string, BlobData> m_Data;
	};

	class ColumnExpression final : public IOperationExpression
	{
	public:
		ColumnExpression(const ColumnDefinition& column);

		void GenerateStatement(IStatementGenerator& gen) const override;
		std::unique_ptr<IOperationExpression> Clone() const override;

		const ColumnDefinition& m_Column;
	};

	struct Column2;

	template<typename T> struct ColumnDataSerializer;

	template<typename T>
	concept ColumnDataSerializable = requires(T x)
	{
		{ ColumnDataSerializer<std::decay_t<T>>::Serialize(x) } -> ConstantExpressionValue;
		{ ColumnDataSerializer<std::decay_t<T>>::Deserialize(std::declval<const Column2&>()) } -> std::same_as<std::decay_t<T>>;
	};

	std::unique_ptr<ColumnExpression> CreateOperationExpression(const ColumnDefinition& column);

	template<ConstantExpressionValue TValue>
	std::unique_ptr<ConstantExpression> CreateOperationExpression(TValue&& value)
	{
		auto expr = std::make_unique<ConstantExpression>();

		using TValueDecay = std::decay_t<TValue>;

		if constexpr (std::is_integral_v<TValueDecay>)
			expr->m_Data.emplace<int64_t>(value);
		else if constexpr (std::is_floating_point_v<TValueDecay>)
			expr->m_Data.emplace<double>(value);
		else if constexpr (std::is_same_v<TValueDecay, BlobData>)
			expr->m_Data.emplace<BlobData>(value);
		else
			expr->m_Data.emplace<std::string>(std::string(std::forward<TValue>(value)));

		return expr;
	}

	template<ColumnDataSerializable TValue>
	std::unique_ptr<ConstantExpression> CreateOperationExpression(TValue&& value)
	{
		return CreateOperationExpression(ColumnDataSerializer<std::decay_t<TValue>>::Serialize(std::forward<TValue>(value)));
	}

	template<typename T>
	concept OperationExpressionValue =
		ConstantExpressionValue<T> ||
		std::same_as<T, ColumnDefinition> ||
		ColumnDataSerializable<T>;

	struct BinaryOperation final : IOperationExpression
	{
		BinaryOperation(BinaryOperator operation, std::unique_ptr<IOperationExpression> lhs, std::unique_ptr<IOperationExpression> rhs);

		void GenerateStatement(IStatementGenerator& gen) const override;
		std::unique_ptr<IOperationExpression> Clone() const override;

		friend BinaryOperation operator==(const BinaryOperation& lhs, const BinaryOperation& rhs);
		friend BinaryOperation operator!=(const BinaryOperation& lhs, const BinaryOperation& rhs);
		friend BinaryOperation operator&&(const BinaryOperation& lhs, const BinaryOperation& rhs);
		friend BinaryOperation operator||(const BinaryOperation& lhs, const BinaryOperation& rhs);

		std::unique_ptr<IOperationExpression> m_LHS;
		std::unique_ptr<IOperationExpression> m_RHS;
		BinaryOperator m_Operation;
	};

#define OP_EXPR(op, opName) \
	template<OperationExpressionValue TValue> \
	friend BinaryOperation operator op(const ColumnDefinition& lhs, const TValue& rhs) \
	{ \
		return BinaryOperation(BinaryOperator::opName, CreateOperationExpression(lhs), CreateOperationExpression(rhs)); \
	} \
	template<OperationExpressionValue TValue> \
	friend BinaryOperation operator op(const TValue& lhs, const ColumnDefinition& rhs) \
	{ \
		return BinaryOperation(BinaryOperator::opName, CreateOperationExpression(lhs), CreateOperationExpression(rhs)); \
	}

	struct ColumnDefinition
	{
		constexpr ColumnDefinition(const char* name, ColumnType type, ColumnFlags flags = ColumnFlags::None) :
			m_Name(name), m_Type(type), m_Flags(flags)
		{
		}

		OP_EXPR(==, Equals);
		OP_EXPR(!=, NotEquals);
		OP_EXPR(>, GreaterThan);
		OP_EXPR(>=, GreaterThanOrEqual);
		OP_EXPR(<, LessThan);
		OP_EXPR(<=, LessThanOrEqual);

		const char* m_Name = nullptr;
		ColumnType m_Type{};
		ColumnFlags m_Flags{};
	};

#undef OP_EXPR

	struct Column2 : SQLite::Column
	{
		Column2(SQLite::Column&& innerColumn);

		template<ColumnDataSerializable T>
		operator T() const { return ColumnDataSerializer<std::decay_t<T>>::Deserialize(*this); }
	};

	struct Statement2 : SQLite::Statement
	{
		Statement2(SQLite::Statement&& innerStatement);

		Column2 getColumn(const ColumnDefinition& definition);
	};

	class SelectStatementBuilder
	{
	public:
		explicit SelectStatementBuilder(const std::string_view& tableName);

		SelectStatementBuilder& Where(BinaryOperation&& binOp);

		Statement2 Run(const SQLite::Database& db) const;

	private:
		std::string m_TableName;
		std::optional<BinaryOperation> m_WhereCondition;
	};

	struct ColumnData
	{
		ColumnData(const ColumnDefinition& column, uint32_t intData);
		ColumnData(const ColumnDefinition& column, int32_t intData);
		ColumnData(const ColumnDefinition& column, int64_t intData);
		ColumnData(const ColumnDefinition& column, double realData);
		ColumnData(const ColumnDefinition& column, const char* textData);
		ColumnData(const ColumnDefinition& column, const BlobData& blobData);
		ColumnData(const ColumnDefinition& column, std::nullptr_t null);

		template<ColumnDataSerializable T>
		ColumnData(const ColumnDefinition& column, const T& data) :
			ColumnData(column, ColumnDataSerializer<std::decay_t<T>>::Serialize(data))
		{
		}

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
}
