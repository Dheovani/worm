#include <core/expression.hpp>

#include <errors/invalid-arg-exception.hpp>

#include <string>
#include <utility>

namespace
{
	void ValidateColumn(std::string_view column)
	{
		if (column.empty())
			throw worm::InvalidArgException("Expression column cannot be empty.");
	}

	std::string_view ComparisonOperator(worm::core::Comparison comparison)
	{
		using enum worm::core::Comparison;

		switch (comparison) {
		case Equal:          return " = ";
		case NotEqual:       return " <> ";
		case Greater:        return " > ";
		case GreaterOrEqual: return " >= ";
		case Less:           return " < ";
		case LessOrEqual:    return " <= ";
		case Like:           return " LIKE ";
		}

		throw worm::InvalidArgException("Unsupported comparison operator.");
	}

	worm::core::Expression MembershipExpression(
		std::string_view column,
		std::vector<worm::core::Parameter> values,
		std::string_view keyword
	)
	{
		ValidateColumn(column);
		if (values.empty())
			throw worm::InvalidArgException("Membership expressions require at least one value.");

		std::string sql{column};
		sql += keyword;
		sql += " (";

		for (std::size_t index = 0; index < values.size(); ++index) {
			if (index != 0)
				sql += ", ";
			sql += '?';
		}

		sql += ')';
		return {std::move(sql), std::move(values)};
	}
}

worm::core::Expression worm::core::Expressions::Compare(
	std::string_view column,
	Comparison comparison,
	Parameter value
)
{
	ValidateColumn(column);
	return {
		std::string{column} + std::string{ComparisonOperator(comparison)} + "?",
		{std::move(value)}
	};
}

worm::core::Expression worm::core::Expressions::IsNull(std::string_view column)
{
	ValidateColumn(column);
	return {std::string{column} + " IS NULL", {}};
}

worm::core::Expression worm::core::Expressions::IsNotNull(std::string_view column)
{
	ValidateColumn(column);
	return {std::string{column} + " IS NOT NULL", {}};
}

worm::core::Expression worm::core::Expressions::Between(
	std::string_view column,
	Parameter lower,
	Parameter upper
)
{
	ValidateColumn(column);
	return {
		std::string{column} + " BETWEEN ? AND ?",
		{std::move(lower), std::move(upper)}
	};
}

worm::core::Expression worm::core::Expressions::In(
	std::string_view column,
	std::vector<Parameter> values
)
{
	return MembershipExpression(column, std::move(values), " IN");
}

worm::core::Expression worm::core::Expressions::NotIn(
	std::string_view column,
	std::vector<Parameter> values
)
{
	return MembershipExpression(column, std::move(values), " NOT IN");
}
