#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <core/query/expression.hpp>
#include <core/query/filter.hpp>
#include <core/query/ordering.hpp>
#include <core/query/source.hpp>
#include <core/query/sql-builder.hpp>
#include <core/query/statement.hpp>
#include <utils/dependency-injection.hpp>

namespace worm::core
{

  class QueryBuilder final
  {
  public:
    explicit QueryBuilder()
      : sqlBuilder(worm::DependencyInjector<SqlBuilder>().get())
    {}

    explicit QueryBuilder(const SqlBuilder& sqlBuilder) noexcept
      : sqlBuilder(sqlBuilder)
    {}

    [[nodiscard]]
    Statement selectAll(const Source& source,
      const std::vector<Relation>& relations = {},
      const std::optional<Filter>& filter = std::nullopt,
      const std::vector<Ordering>& ordering = {}) const
    {
      return sqlBuilder.selectAll(source, relations, filter, ordering);
    }

    [[nodiscard]]
    Statement select(const std::vector<worm::core::Field>& fields,
      const Source& source,
      const std::vector<Relation>& relations = {},
      const std::optional<Filter>& filter = std::nullopt,
      const std::vector<Ordering>& ordering = {}) const
    {
      return sqlBuilder.select(fields, source, relations, filter, ordering);
    }

    [[nodiscard]]
    Statement insert(const Source& source, const std::vector<std::pair<std::string, Parameter>>& columns) const
    {
      return sqlBuilder.insert(source, columns);
    }

    [[nodiscard]]
    Statement insertFromSelect(
      const Source& target, const std::vector<std::string>& targetColumns, const Statement& sourceStatement) const
    {
      return sqlBuilder.insertFromSelect(target, targetColumns, sourceStatement);
    }

    [[nodiscard]]
    Statement insertFromSelect(const Source& target,
      const std::vector<std::string>& targetColumns,
      const std::vector<Field>& selectedFields,
      const Source& source,
      const std::vector<Relation>& relations = {},
      const std::optional<Filter>& filter = std::nullopt,
      const std::vector<Ordering>& ordering = {}) const
    {
      return sqlBuilder.insertFromSelect(target, targetColumns, selectedFields, source, relations, filter, ordering);
    }

    [[nodiscard]]
    Statement update(const Source& source,
      const std::vector<std::pair<std::string, Parameter>>& columns,
      const std::optional<Filter>& filter = std::nullopt) const
    {
      return sqlBuilder.update(source, columns, filter);
    }

    [[nodiscard]]
    Statement delete_(const Source& source, const std::optional<Filter>& filter = std::nullopt) const
    {
      return sqlBuilder.delete_(source, filter);
    }

  private:
    const SqlBuilder& sqlBuilder;
  };

} // namespace worm::core
