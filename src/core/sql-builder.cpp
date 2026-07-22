#include <core/sql-builder.hpp>

#include <connection/client.hpp>
#include <errors/database-exception.hpp>
#include <errors/invalid-arg-exception.hpp>
#include <utils/dependency-injection.hpp>

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
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

    std::vector<Parameter> extractParameters(
      std::initializer_list<std::reference_wrapper<const std::vector<Parameter>>> params)
    {
      std::vector<Parameter> parameters;
      std::size_t size = 0;

      for (const auto& param : params) {
        size += param.get().size();
      }

      parameters.reserve(size);

      for (const auto& param : params) {
        parameters.insert(parameters.end(), param.get().begin(), param.get().end());
      }

      return parameters;
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

    std::string listSelectFields(const std::vector<worm::core::Field>& fields)
    {
      std::string list;

      for (std::size_t index = 0; index < fields.size(); ++index) {
        const auto& field = fields[index];

        list += std::string{field.source.alias.value_or(field.source.name)};
        list += ".";
        list += std::string{field.name};

        if (index + 1 < fields.size()) {
          list += ",";
        }
      }

      return list;
    }

    std::string getJoinClause(const Join type)
    {
      using enum Join;

      switch (type) {
      case Inner:
        return "inner join";
      case Left:
        return "left join";
      case Right:
        return "right join";
      case Full:
        return "full join";
      }

      return "inner join";
    }

    std::string buildRelations(
      const Builder& builder,
      const std::vector<Relation>& relations)
    {
      std::string list;
      std::size_t parameterIndex = 1;

      for (auto& rel : relations) {
        list += getJoinClause(rel.joinType) + " ";
        list += std::string{rel.joinedSource.name} + " ";

        if (rel.joinedSource.alias.has_value()) {
          list += std::string{rel.joinedSource.alias.value()};
        }

        list += " on (";
        list += builder.renderExpression(rel.condition, parameterIndex);
        list += ")";
        parameterIndex += rel.condition.parameters.size();
      }

      return list;
    }

  } // namespace

  Expression Builder::compare(
    std::string_view column,
    Comparison comparison,
    Parameter value) const
  {
    validateColumn(column);
    return {std::string{column} + std::string{comparisonOperator(comparison)} + "?", {std::move(value)}};
  }

  Expression Builder::isNull(std::string_view column) const
  {
    validateColumn(column);
    return {std::string{column} + " IS NULL", {}};
  }

  Expression Builder::isNotNull(std::string_view column) const
  {
    validateColumn(column);
    return {std::string{column} + " IS NOT NULL", {}};
  }

  Expression Builder::between(
    std::string_view column,
    Parameter lower,
    Parameter upper) const
  {
    validateColumn(column);
    return {std::string{column} + " BETWEEN ? AND ?", {std::move(lower), std::move(upper)}};
  }

  Expression Builder::in(
    std::string_view column,
    std::vector<Parameter> values) const
  {
    return membershipExpression(column, std::move(values), " IN");
  }

  Expression Builder::notIn(
    std::string_view column,
    std::vector<Parameter> values) const
  {
    return membershipExpression(column, std::move(values), " NOT IN");
  }

  Expression Builder::_and(
    const Expression& left,
    const Expression& right) const
  {
    return {
      left.sql + " and " + right.sql,
      extractParameters({std::cref(left.parameters), std::cref(right.parameters)})};
  }

  Expression Builder::_or(
    const Expression& left,
    const Expression& right) const
  {
    return {
      left.sql + " or " + right.sql,
      extractParameters({std::cref(left.parameters), std::cref(right.parameters)})};
  }

  std::string Builder::placeholder(std::size_t) const
  {
    return "?";
  }

  std::string Builder::renderExpression(
    const Expression& expression,
    std::size_t firstParameterIndex) const
  {
    std::string rendered;
    rendered.reserve(expression.sql.size());

    std::size_t parameterIndex = firstParameterIndex;
    for (const char character : expression.sql) {
      if (character == '?') {
        rendered += placeholder(parameterIndex);
        ++parameterIndex;
      } else {
        rendered += character;
      }
    }

    return rendered;
  }

  std::string Builder::select(
    const std::vector<worm::core::Field>& fields,
    const Source& source,
    const std::vector<Relation>& relations) const
  {
    const std::string fieldsList = listSelectFields(fields);
    const std::string sourceName = std::string{source.name} + " " + std::string{source.alias.value_or("")};
    const std::string _relations = buildRelations(*this, relations);
    const std::string sql = "select " + fieldsList + " from " + sourceName + " " + _relations;

    return sql;
  }

  std::string PgBuilder::placeholder(std::size_t index) const
  {
    return "$" + std::to_string(index);
  }

  std::unique_ptr<Builder> getBuilder()
  {
    const auto type = worm::DependencyInjector<worm::connection::DatabaseType>().get();

    switch (type) {
    case worm::connection::DatabaseType::PostgreSQL:
      return std::make_unique<PgBuilder>();
    case worm::connection::DatabaseType::MySQL:
      return std::make_unique<MySqlBuilder>();
    case worm::connection::DatabaseType::SQLite:
      return std::make_unique<SqliteBuilder>();
    default:
      throw worm::DatabaseException("Unsupported database type.");
    }
  }

} // namespace worm::core
