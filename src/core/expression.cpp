#include <core/expression.hpp>

#include <errors/invalid-arg-exception.hpp>

#include <string>
#include <utility>

namespace
{
  void validateColumn(std::string_view column)
  {
    if (column.empty())
      throw worm::InvalidArgException("Expression column cannot be empty.");
  }

  std::string_view comparisonOperator(worm::core::Comparison comparison)
  {
    using enum worm::core::Comparison;

    switch (comparison) {
    case Equal:
      return " = ";
    case NotEqual:
      return " <> ";
    case Greater:
      return " > ";
    case GreaterOrEqual:
      return " >= ";
    case Less:
      return " < ";
    case LessOrEqual:
      return " <= ";
    case Like:
      return " LIKE ";
    }

    throw worm::InvalidArgException("Unsupported comparison operator.");
  }

  worm::core::Expression membershipExpression(std::string_view column,
                                              std::vector<worm::core::Parameter> values,
                                              std::string_view keyword)
  {
    validateColumn(column);
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
} // namespace

worm::core::Expression worm::core::Expressions::compare(std::string_view column,
                                                        Comparison comparison, Parameter value)
{
  validateColumn(column);
  return {std::string{column} + std::string{comparisonOperator(comparison)} + "?",
          {std::move(value)}};
}

worm::core::Expression worm::core::Expressions::isNull(std::string_view column)
{
  validateColumn(column);
  return {std::string{column} + " IS NULL", {}};
}

worm::core::Expression worm::core::Expressions::isNotNull(std::string_view column)
{
  validateColumn(column);
  return {std::string{column} + " IS NOT NULL", {}};
}

worm::core::Expression worm::core::Expressions::between(std::string_view column, Parameter lower,
                                                        Parameter upper)
{
  validateColumn(column);
  return {std::string{column} + " BETWEEN ? AND ?", {std::move(lower), std::move(upper)}};
}

worm::core::Expression worm::core::Expressions::in(std::string_view column,
                                                   std::vector<Parameter> values)
{
  return membershipExpression(column, std::move(values), " IN");
}

worm::core::Expression worm::core::Expressions::notIn(std::string_view column,
                                                      std::vector<Parameter> values)
{
  return membershipExpression(column, std::move(values), " NOT IN");
}
