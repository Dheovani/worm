#pragma once

#include <concepts>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <core/query/clauses.hpp>
#include <core/query/criteria.hpp>
#include <core/query/expression.hpp>
#include <core/query/filter.hpp>
#include <core/query/pagination.hpp>
#include <core/query/statement.hpp>

namespace worm::core
{

  class SqlBuilder
  {
  public:
    [[nodiscard]]
    virtual Statement selectAll(const Source& source,
      const std::vector<Relation>& relations,
      const std::optional<Filter>& filter = std::nullopt,
      const std::vector<Ordering>& ordering = {},
      const std::optional<Pagination>& pagination = std::nullopt,
      const std::vector<Grouping>& grouping = {},
      const std::optional<Filter>& having = std::nullopt) const;

    [[nodiscard]]
    Statement selectAll(const Source& source, const Criteria& criteria) const;

    [[nodiscard]]
    virtual Statement select(const std::vector<worm::core::Field>& fields,
      const Source& source,
      const std::vector<Relation>& relations,
      const std::optional<Filter>& filter = std::nullopt,
      const std::vector<Ordering>& ordering = {},
      const std::optional<Pagination>& pagination = std::nullopt,
      const std::vector<Grouping>& grouping = {},
      const std::optional<Filter>& having = std::nullopt) const;

    [[nodiscard]]
    Statement select(
      const std::vector<worm::core::Field>& fields, const Source& source, const Criteria& criteria) const;

    [[nodiscard]]
    virtual Statement insert(const Source& source, const std::vector<std::pair<std::string, Parameter>>& columns) const;

    [[nodiscard]]
    virtual Statement insertFromSelect(
      const Source& target, const std::vector<std::string>& targetColumns, const Statement& sourceStatement) const;

    [[nodiscard]]
    virtual Statement insertFromSelect(const Source& target,
      const std::vector<std::string>& targetColumns,
      const std::vector<Field>& selectedFields,
      const Source& source,
      const std::vector<Relation>& relations,
      const std::optional<Filter>& filter = std::nullopt,
      const std::vector<Ordering>& ordering = {},
      const std::optional<Pagination>& pagination = std::nullopt,
      const std::vector<Grouping>& grouping = {},
      const std::optional<Filter>& having = std::nullopt) const;

    [[nodiscard]]
    Statement insertFromSelect(const Source& target,
      const std::vector<std::string>& targetColumns,
      const std::vector<Field>& selectedFields,
      const Source& source,
      const Criteria& criteria) const;

    [[nodiscard]]
    virtual Statement update(const Source& source,
      const std::vector<std::pair<std::string, Parameter>>& columns,
      const std::optional<Filter>& filter = std::nullopt) const;

    [[nodiscard]]
    virtual Statement delete_(const Source& source, const std::optional<Filter>& filter = std::nullopt) const;

    virtual ~SqlBuilder() = default;

  protected:
    [[nodiscard]]
    virtual std::string placeholder(std::size_t index) const;

    [[nodiscard]]
    virtual std::string renderMutationSource(const Source& source) const;

    [[nodiscard]]
    virtual std::string renderUpdateFrom(const Source& source) const;

    [[nodiscard]]
    virtual std::string renderDeletePrefix(const Source& source) const;

    [[nodiscard]]
    virtual Expression renderPagination(
      const Pagination& pagination, std::size_t firstParameterIndex, bool hasOrdering) const;

  private:
    [[nodiscard]]
    std::string renderExpression(const Expression& expression, std::size_t firstParameterIndex = 1) const;

    [[nodiscard]]
    std::string buildRelations(const std::vector<Relation>& relations) const;

    [[nodiscard]]
    std::string renderFilter(const Filter& filter, std::size_t firstParameterIndex = 1) const;

    [[nodiscard]]
    std::string renderGrouping(const std::vector<Grouping>& grouping) const;

    [[nodiscard]]
    std::string renderOrdering(const std::vector<Ordering>& ordering) const;
  };

  class PgBuilder : public SqlBuilder
  {
  protected:
    [[nodiscard]]
    std::string placeholder(std::size_t index) const override;
  };

  class MySqlBuilder : public SqlBuilder
  {};

  class SqliteBuilder : public SqlBuilder
  {
  protected:
    [[nodiscard]]
    std::string renderMutationSource(const Source& source) const override;
  };

  class SqlServerBuilder : public SqlBuilder
  {
  protected:
    [[nodiscard]]
    std::string renderMutationSource(const Source& source) const override;

    [[nodiscard]]
    std::string renderUpdateFrom(const Source& source) const override;

    [[nodiscard]]
    std::string renderDeletePrefix(const Source& source) const override;

    [[nodiscard]]
    Expression renderPagination(
      const Pagination& pagination, std::size_t firstParameterIndex, bool hasOrdering) const override;
  };

  template <typename T>
  concept SqlBuilderI = std::derived_from<std::remove_cvref_t<T>, SqlBuilder>;

  [[nodiscard]]
  std::unique_ptr<SqlBuilder> getSqlBuilder();

} // namespace worm::core
