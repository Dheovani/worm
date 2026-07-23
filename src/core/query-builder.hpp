#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <core/filter.hpp>
#include <core/ordering.hpp>
#include <core/source.hpp>
#include <core/sql-builder.hpp>

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
    std::string select(
      const std::vector<worm::core::Field>& fields,
      const Source& source,
      const std::vector<Relation>& relations = {},
      const std::optional<Filter>& filter = std::nullopt,
      const std::vector<Ordering>& ordering = {}) const
    {
      return sqlBuilder_.select(fields, source, relations, filter, ordering);
    }

    [[nodiscard]]
    std::string insert(
      const Source& source,
      const std::vector<std::pair<std::string, Parameter>>& columns) const
    {
      return sqlBuilder_.insert(source, columns);
    }

    [[nodiscard]]
    std::string insertFromSelect(
      const Source& target,
      const std::vector<std::string>& targetColumns,
      const std::string& sourceQuery) const
    {
      return sqlBuilder_.insertFromSelect(target, targetColumns, sourceQuery);
    }

    [[nodiscard]]
    std::string insertFromSelect(
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
    std::string update(
      const Source& source,
      const std::vector<std::pair<std::string, Parameter>>& columns,
      const std::optional<Filter>& filter = std::nullopt) const
    {
      return sqlBuilder_.update(source, columns, filter);
    }

    [[nodiscard]]
    std::string delete_(const Source& source, const std::optional<Filter>& filter = std::nullopt) const
    {
      return sqlBuilder_.delete_(source, filter);
    }

  private:
    const SqlBuilderType& sqlBuilder_;
  };

} // namespace worm::core
