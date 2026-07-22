#include <core/expression.hpp>

#include <errors/invalid-arg-exception.hpp>

#include <initializer_list>
#include <string>
#include <utility>

namespace worm::core
{

  namespace
  {

    void validateColumn(std::string_view column)
    {
      if (column.empty())
        throw worm::InvalidArgException("Expression column cannot be empty.");
    }

    std::string_view comparisonOperator(Comparison comparison)
    {
      using enum Comparison;

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

    Expression membershipExpression(std::string_view column, std::vector<Parameter> values, std::string_view keyword)
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

    std::vector<Parameter> extractParameters(std::initializer_list<std::vector<Parameter>> params)
    {
      std::vector<Parameter> parameters;
      int size = 0;

      for (auto& param : params)
        size += param.size();

      parameters.reserve(size);

      for (auto& param : params)
        parameters.insert(parameters.end(), params.begin(), params.end());

      return parameters;
    }

  } // namespace

  Expression ExpressionBuilder::compare(std::string_view column, Comparison comparison, Parameter value)
  {
    validateColumn(column);
    return {std::string{column} + std::string{comparisonOperator(comparison)} + "?", {std::move(value)}};
  }

  Expression ExpressionBuilder::isNull(std::string_view column)
  {
    validateColumn(column);
    return {std::string{column} + " IS NULL", {}};
  }

  Expression ExpressionBuilder::isNotNull(std::string_view column)
  {
    validateColumn(column);
    return {std::string{column} + " IS NOT NULL", {}};
  }

  Expression ExpressionBuilder::between(std::string_view column, Parameter lower, Parameter upper)
  {
    validateColumn(column);
    return {std::string{column} + " BETWEEN ? AND ?", {std::move(lower), std::move(upper)}};
  }

  Expression ExpressionBuilder::in(std::string_view column, std::vector<Parameter> values)
  {
    return membershipExpression(column, std::move(values), " IN");
  }

  Expression ExpressionBuilder::notIn(std::string_view column, std::vector<Parameter> values)
  {
    return membershipExpression(column, std::move(values), " NOT IN");
  }

  Expression ExpressionBuilder::_and(Expression& left, Expression& right)
  {
    std::vector<Parameter> params;
    params = extractParameters(std::initializer_list{left.parameters, right.parameters});

    return Expression{.sql = left.sql + " and " + right.sql, .parameters = params};
  }

  Expression ExpressionBuilder::_or(Expression& left, Expression& right)
  {
    std::vector<Parameter> params;
    params = extractParameters(std::initializer_list{left.parameters, right.parameters});

    return Expression{.sql = left.sql + " or " + right.sql, .parameters = params};
  }

} // namespace worm::core
