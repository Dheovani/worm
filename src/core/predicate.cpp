#include <core/predicate.hpp>

#include <errors/invalid-arg-exception.hpp>

#include <cstddef>
#include <string>
#include <utility>

namespace worm::core
{

  namespace
  {
    void validateColumn(std::string_view column)
    {
      if (column.empty()) {
        throw worm::InvalidArgException("Expression column cannot be empty.");
      }
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

    Expression membershipExpression(
      std::string_view column,
      std::vector<Parameter> values,
      std::string_view keyword)
    {
      validateColumn(column);
      if (values.empty()) {
        throw worm::InvalidArgException("Membership expressions require at least one value.");
      }

      std::string sql{column};
      sql += keyword;
      sql += " (";

      for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
          sql += ", ";
        }
        sql += '?';
      }

      sql += ')';
      return {std::move(sql), std::move(values)};
    }

    Expression logicalExpression(
      const std::vector<Expression>& expressions,
      std::string_view logicalOperator)
    {
      if (expressions.empty()) {
        throw worm::InvalidArgException("Logical expressions require at least one expression.");
      }

      std::string sql;
      std::vector<Parameter> parameters;

      for (const auto& expression : expressions) {
        parameters.insert(parameters.end(), expression.parameters.begin(), expression.parameters.end());
      }

      for (std::size_t index = 0; index < expressions.size(); ++index) {
        if (index != 0) {
          sql += " ";
          sql += logicalOperator;
          sql += " ";
        }

        sql += "(";
        sql += expressions[index].sql;
        sql += ")";
      }

      return {std::move(sql), std::move(parameters)};
    }
  } // namespace

  Expression Predicate::compare(std::string_view column, Comparison comparison, Parameter value)
  {
    validateColumn(column);
    return {std::string{column} + std::string{comparisonOperator(comparison)} + "?", {std::move(value)}};
  }

  Expression Predicate::equal(std::string_view column, Parameter value)
  {
    return compare(column, Comparison::Equal, std::move(value));
  }

  Expression Predicate::isNull(std::string_view column)
  {
    validateColumn(column);
    return {std::string{column} + " IS NULL", {}};
  }

  Expression Predicate::isNotNull(std::string_view column)
  {
    validateColumn(column);
    return {std::string{column} + " IS NOT NULL", {}};
  }

  Expression Predicate::between(std::string_view column, Parameter lower, Parameter upper)
  {
    validateColumn(column);
    return {std::string{column} + " BETWEEN ? AND ?", {std::move(lower), std::move(upper)}};
  }

  Expression Predicate::in(std::string_view column, std::vector<Parameter> values)
  {
    return membershipExpression(column, std::move(values), " IN");
  }

  Expression Predicate::notIn(std::string_view column, std::vector<Parameter> values)
  {
    return membershipExpression(column, std::move(values), " NOT IN");
  }

  Expression Predicate::not_(const Expression& expression)
  {
    return {
      "not (" + expression.sql + ")",
      expression.parameters};
  }

  Expression Predicate::all(const Expression& left, const Expression& right)
  {
    return all({left, right});
  }

  Expression Predicate::all(const std::vector<Expression>& expressions)
  {
    return logicalExpression(expressions, "and");
  }

  Expression Predicate::any(const Expression& left, const Expression& right)
  {
    return any({left, right});
  }

  Expression Predicate::any(const std::vector<Expression>& expressions)
  {
    return logicalExpression(expressions, "or");
  }

} // namespace worm::core
