#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <core/query/clauses.hpp>
#include <core/query/criteria.hpp>
#include <core/query/expression.hpp>
#include <core/query/filter.hpp>
#include <core/query/pagination.hpp>
#include <core/query/sql-builder.hpp>
#include <core/query/statement.hpp>

namespace worm::core
{

  class QueryBuilder final
  {
  public:
    explicit QueryBuilder();

    explicit QueryBuilder(const SqlBuilder& sqlBuilder) noexcept;

    [[nodiscard]]
    Statement selectAll(
      const Source& source,
      const std::vector<Relation>& relations = {},
      const std::optional<Filter>& filter = std::nullopt,
      const std::vector<Ordering>& ordering = {},
      const std::optional<Pagination>& pagination = std::nullopt,
      const std::vector<Grouping>& grouping = {},
      const std::optional<Filter>& having = std::nullopt) const;

    [[nodiscard]]
    Statement selectAll(const Source& source, const Criteria& criteria) const;

    [[nodiscard]]
    Statement select(
      const std::vector<worm::core::Field>& fields,
      const Source& source,
      const std::vector<Relation>& relations = {},
      const std::optional<Filter>& filter = std::nullopt,
      const std::vector<Ordering>& ordering = {},
      const std::optional<Pagination>& pagination = std::nullopt,
      const std::vector<Grouping>& grouping = {},
      const std::optional<Filter>& having = std::nullopt) const;

    [[nodiscard]]
    Statement select(
      const std::vector<worm::core::Field>& fields, const Source& source, const Criteria& criteria) const;

    [[nodiscard]]
    Statement insert(const Source& source, const std::vector<std::pair<std::string, Parameter>>& columns) const;

    [[nodiscard]]
    Statement insertFromSelect(
      const Source& target, const std::vector<std::string>& targetColumns, const Statement& sourceStatement) const;

    [[nodiscard]]
    Statement insertFromSelect(
      const Source& target,
      const std::vector<std::string>& targetColumns,
      const std::vector<Field>& selectedFields,
      const Source& source,
      const std::vector<Relation>& relations = {},
      const std::optional<Filter>& filter = std::nullopt,
      const std::vector<Ordering>& ordering = {},
      const std::optional<Pagination>& pagination = std::nullopt,
      const std::vector<Grouping>& grouping = {},
      const std::optional<Filter>& having = std::nullopt) const;

    [[nodiscard]]
    Statement insertFromSelect(
      const Source& target,
      const std::vector<std::string>& targetColumns,
      const std::vector<Field>& selectedFields,
      const Source& source,
      const Criteria& criteria) const;

    [[nodiscard]]
    Statement update(
      const Source& source,
      const std::vector<std::pair<std::string, Parameter>>& columns,
      const std::optional<Filter>& filter = std::nullopt) const;

    [[nodiscard]]
    Statement delete_(const Source& source, const std::optional<Filter>& filter = std::nullopt) const;

  private:
    const SqlBuilder& sqlBuilder_;
  };

} // namespace worm::core
