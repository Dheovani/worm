#include <core/sql-builder.hpp>

#include <connection/client.hpp>
#include <errors/database-exception.hpp>
#include <utils/dependency-injection.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>

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

    for (auto& rel : relations) {
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

  std::string SqlBuilder::select(
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
    return sql;
  }

  std::string SqlBuilder::insert(const Source&) const
  {
    throw worm::DatabaseException("Insert query building is not implemented yet.");
  }

  std::string SqlBuilder::update(const Source&, const std::optional<Filter>&) const
  {
    throw worm::DatabaseException("Update query building is not implemented yet.");
  }

  std::string SqlBuilder::remove(const Source&, const std::optional<Filter>&) const
  {
    throw worm::DatabaseException("Delete query building is not implemented yet.");
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
      throw worm::DatabaseException("Unsupported database type.");
    }
  }

} // namespace worm::core
