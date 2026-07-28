#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <core/query/filter.hpp>
#include <core/query/ordering.hpp>
#include <core/query/source.hpp>
#include <core/query/sql-builder.hpp>

namespace worm::core
{

  template <SqlBuilderI SqlBuilderType>
  class QueryBuilder
  {
  public:
    explicit QueryBuilder(const SqlBuilderType& sqlBuilder) noexcept
      : sqlBuilder_(sqlBuilder)
    {}

    [[nodiscard]]
    Statement select(
      const std::vector<worm::core::Field>& fields,
      const Source& source,
      const std::vector<Relation>& relations = {},
      const std::optional<Filter>& filter = std::nullopt,
      const std::vector<Ordering>& ordering = {}) const
    {
      return sqlBuilder_.select(fields, source, relations, filter, ordering);
    }

    [[nodiscard]]
    Statement insert(
      const Source& source,
      const std::vector<std::pair<std::string, Parameter>>& columns) const
    {
      return sqlBuilder_.insert(source, columns);
    }

    [[nodiscard]]
    Statement insertFromSelect(
      const Source& target,
      const std::vector<std::string>& targetColumns,
      const Statement& sourceStatement) const
    {
      return sqlBuilder_.insertFromSelect(target, targetColumns, sourceStatement);
    }

    [[nodiscard]]
    Statement insertFromSelect(
      const Source& target,
      const std::vector<std::string>& targetColumns,
      const std::vector<Field>& selectedFields,
      const Source& source,
      const std::vector<Relation>& relations = {},
      const std::optional<Filter>& filter = std::nullopt,
      const std::vector<Ordering>& ordering = {}) const
    {
      return sqlBuilder_.insertFromSelect(
        target,
        targetColumns,
        selectedFields,
        source,
        relations,
        filter,
        ordering);
    }

    [[nodiscard]]
    Statement update(
      const Source& source,
      const std::vector<std::pair<std::string, Parameter>>& columns,
      const std::optional<Filter>& filter = std::nullopt) const
    {
      return sqlBuilder_.update(source, columns, filter);
    }

    [[nodiscard]]
    Statement delete_(const Source& source, const std::optional<Filter>& filter = std::nullopt) const
    {
      return sqlBuilder_.delete_(source, filter);
    }

  private:
    const SqlBuilderType& sqlBuilder_;
  };

} // namespace worm::core
