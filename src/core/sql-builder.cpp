#include <core/sql-builder.hpp>

#include <connection/client.hpp>
#include <errors/sql-build-exception.hpp>
#include <errors/unsupported-database-exception.hpp>
#include <utils/dependency-injection.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace worm::core
{

  namespace
  {

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

    std::string getOrderDirection(const OrderDirection direction)
    {
      using enum OrderDirection;

      switch (direction) {
      case Ascending:
        return "asc";
      case Descending:
        return "desc";
      }

      return "asc";
    }

    void appendParameters(std::vector<Parameter>& target, const std::vector<Parameter>& source)
    {
      target.insert(target.end(), source.begin(), source.end());
    }

    std::vector<Parameter> relationParameters(const std::vector<Relation>& relations)
    {
      std::vector<Parameter> parameters;

      for (const auto& relation : relations) {
        appendParameters(parameters, relation.condition.parameters);
      }

      return parameters;
    }

    std::vector<Parameter> columnParameters(const std::vector<std::pair<std::string, Parameter>>& columns)
    {
      std::vector<Parameter> parameters;
      parameters.reserve(columns.size());

      for (const auto& column : columns) {
        parameters.push_back(column.second);
      }

      return parameters;
    }

  } // namespace

  std::string SqlBuilder::placeholder(std::size_t) const
  {
    return "?";
  }

  std::string SqlBuilder::renderExpression(
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

  std::string SqlBuilder::buildRelations(const std::vector<Relation>& relations) const
  {
    std::string list;
    std::size_t parameterIndex = 1;

    for (std::size_t index = 0; index < relations.size(); ++index) {
      const auto& rel = relations[index];

      if (index != 0) {
        list += " ";
      }

      list += getJoinClause(rel.joinType) + " ";
      list += std::string{rel.joinedSource.name} + " ";

      if (rel.joinedSource.alias.has_value()) {
        list += std::string{rel.joinedSource.alias.value()};
      }

      list += " on (";
      list += renderExpression(rel.condition, parameterIndex);
      list += ")";
      parameterIndex += rel.condition.parameters.size();
    }

    return list;
  }

  std::string SqlBuilder::renderFilter(
    const Filter& filter,
    std::size_t firstParameterIndex) const
  {
    return renderExpression(filter.expression(), firstParameterIndex);
  }

  std::string SqlBuilder::renderOrdering(const std::vector<Ordering>& ordering) const
  {
    if (ordering.empty()) {
      return {};
    }

    std::string sql = " order by ";
    for (std::size_t index = 0; index < ordering.size(); ++index) {
      const auto& order = ordering[index];

      if (index != 0) {
        sql += ",";
      }

      sql += std::string{order.column};
      sql += " ";
      sql += getOrderDirection(order.direction);
    }

    return sql;
  }

  Statement SqlBuilder::select(
    const std::vector<worm::core::Field>& fields,
    const Source& source,
    const std::vector<Relation>& relations,
    const std::optional<Filter>& filter,
    const std::vector<Ordering>& ordering) const
  {
    const std::string fieldsList = listSelectFields(fields);
    const std::string sourceName = std::string{source.name} + " " + std::string{source.alias.value_or("")};
    const std::string _relations = buildRelations(relations);
    std::string sql = "select " + fieldsList + " from " + sourceName + " " + _relations;

    if (filter.has_value()) {
      std::size_t parameterIndex = 1;
      for (const auto& relation : relations) {
        parameterIndex += relation.condition.parameters.size();
      }

      sql += " where ";
      sql += renderFilter(filter.value(), parameterIndex);
    }

    sql += renderOrdering(ordering);

    std::vector<Parameter> parameters = relationParameters(relations);
    if (filter.has_value()) {
      appendParameters(parameters, filter.value().expression().parameters);
    }

    return {std::move(sql), std::move(parameters)};
  }

  Statement SqlBuilder::insert(
    const Source& source,
    const std::vector<std::pair<std::string, Parameter>>& columns) const
  {
    std::string fields = "(",
                values = "(";

    for (std::size_t index = 0; index < columns.size(); ++index) {
      const auto& field = columns[index].first;

      fields += field;
      values += placeholder(index + 1);

      if (index + 1 < columns.size()) {
        fields += ",";
        values += ",";
      }
    }

    fields += ")";
    values += ")";

    return {
      "insert into " + std::string{source.name} + fields + " values " + values,
      columnParameters(columns)};
  }

  Statement SqlBuilder::insertFromSelect(
    const Source& target,
    const std::vector<std::string>& targetColumns,
    const Statement& sourceStatement) const
  {
    std::string columns = "(";

    for (std::size_t index = 0; index < targetColumns.size(); ++index) {
      const auto& column = targetColumns[index];
      columns += column;

      if (index + 1 < targetColumns.size()) {
        columns += ",";
      }
    }

    columns += ")";

    return {
      "insert into " + std::string{target.name} + columns + " " + sourceStatement.sql,
      sourceStatement.parameters};
  }

  Statement SqlBuilder::insertFromSelect(
    const Source& target,
    const std::vector<std::string>& targetColumns,
    const std::vector<Field>& selectedFields,
    const Source& source,
    const std::vector<Relation>& relations,
    const std::optional<Filter>& filter,
    const std::vector<Ordering>& ordering) const
  {
    const Statement selectStatement = select(selectedFields, source, relations, filter, ordering);

    return insertFromSelect(target, targetColumns, selectStatement);
  }

  Statement SqlBuilder::update(
    const Source& source,
    const std::vector<std::pair<std::string, Parameter>>& columns,
    const std::optional<Filter>& filter) const
  {
    std::string sql = "update " + std::string{source.name};

    if (source.alias.has_value()) {
      sql += " ";
      sql += source.alias.value();
    }

    sql += " set ";

    for (std::size_t index = 0; index < columns.size(); ++index) {
      const auto& field = columns[index].first;

      sql += field;
      sql += " = ";
      sql += placeholder(index + 1);

      if (index + 1 < columns.size()) {
        sql += ",";
      }
    }

    if (filter.has_value()) {
      sql += " where ";
      sql += renderFilter(filter.value(), columns.size() + 1);
    }

    std::vector<Parameter> parameters = columnParameters(columns);
    if (filter.has_value()) {
      appendParameters(parameters, filter.value().expression().parameters);
    }

    return {std::move(sql), std::move(parameters)};
  }

  Statement SqlBuilder::delete_(const Source& source, const std::optional<Filter>& filter) const
  {
    std::string sql = "delete from " + std::string{source.name};

    if (source.alias.has_value()) {
      sql += " ";
      sql += source.alias.value();
    }

    if (filter.has_value()) {
      sql += " where ";
      sql += renderFilter(filter.value(), 1);
    }

    std::vector<Parameter> parameters;
    if (filter.has_value()) {
      parameters = filter.value().expression().parameters;
    }

    return {std::move(sql), std::move(parameters)};
  }

  std::string PgBuilder::placeholder(std::size_t index) const
  {
    return "$" + std::to_string(index);
  }

  std::unique_ptr<SqlBuilder> getSqlBuilder()
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
      throw worm::UnsupportedDatabaseException("Unsupported database type.");
    }
  }

} // namespace worm::core
