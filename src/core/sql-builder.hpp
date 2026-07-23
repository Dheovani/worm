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

#include <core/expression.hpp>
#include <core/filter.hpp>
#include <core/ordering.hpp>
#include <core/source.hpp>
#include <core/statement.hpp>

namespace worm::core
{

  class SqlBuilder
  {
  public:
    [[nodiscard]]
    virtual Statement select(
      const std::vector<worm::core::Field>& fields,
      const Source& source,
      const std::vector<Relation>& relations,
      const std::optional<Filter>& filter = std::nullopt,
      const std::vector<Ordering>& ordering = {}) const;

    [[nodiscard]]
    virtual Statement insert(
      const Source& source,
      const std::vector<std::pair<std::string, Parameter>>& columns) const;

    [[nodiscard]]
    virtual Statement insertFromSelect(
      const Source& target,
      const std::vector<std::string>& targetColumns,
      const Statement& sourceStatement) const;

    [[nodiscard]]
    virtual Statement insertFromSelect(
      const Source& target,
      const std::vector<std::string>& targetColumns,
      const std::vector<Field>& selectedFields,
      const Source& source,
      const std::vector<Relation>& relations,
      const std::optional<Filter>& filter = std::nullopt,
      const std::vector<Ordering>& ordering = {}) const;

    [[nodiscard]]
    virtual Statement update(
      const Source& source,
      const std::vector<std::pair<std::string, Parameter>>& columns,
      const std::optional<Filter>& filter = std::nullopt) const;

    [[nodiscard]]
    virtual Statement delete_(const Source& source, const std::optional<Filter>& filter = std::nullopt) const;

    virtual ~SqlBuilder() = default;

  protected:
    [[nodiscard]]
    virtual std::string placeholder(std::size_t index) const;

  private:
    [[nodiscard]]
    std::string renderExpression(const Expression& expression, std::size_t firstParameterIndex = 1) const;

    [[nodiscard]]
    std::string buildRelations(const std::vector<Relation>& relations) const;

    [[nodiscard]]
    std::string renderFilter(const Filter& filter, std::size_t firstParameterIndex = 1) const;

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
  {};

  template <typename T>
  concept SqlBuilderI = std::derived_from<std::remove_cvref_t<T>, SqlBuilder>;

  [[nodiscard]]
  std::unique_ptr<SqlBuilder> getSqlBuilder();

} // namespace worm::core
