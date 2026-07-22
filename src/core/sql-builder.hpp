#pragma once

#include <concepts>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <core/expression.hpp>
#include <core/source.hpp>

namespace worm::core
{

  class Builder
  {
  public:
    [[nodiscard]]
    virtual std::string select(
      const std::vector<worm::core::Field>& fields,
      const Source& source,
      const std::vector<Relation>& relations) const;

    virtual ~Builder() = default;

    [[nodiscard]]
    Expression compare(
      std::string_view column,
      Comparison comparison,
      Parameter value) const;

    [[nodiscard]]
    Expression isNull(std::string_view column) const;

    [[nodiscard]]
    Expression isNotNull(std::string_view column) const;

    [[nodiscard]]
    Expression between(
      std::string_view column,
      Parameter lower,
      Parameter upper) const;

    [[nodiscard]]
    Expression in(
      std::string_view column,
      std::vector<Parameter> values) const;

    [[nodiscard]]
    Expression notIn(
      std::string_view column,
      std::vector<Parameter> values) const;

    [[nodiscard]]
    Expression _and(
      const Expression& left,
      const Expression& right) const;

    [[nodiscard]]
    Expression _or(
      const Expression& left,
      const Expression& right) const;

    [[nodiscard]]
    std::string renderExpression(
      const Expression& expression,
      std::size_t firstParameterIndex = 1) const;

  protected:
    [[nodiscard]]
    virtual std::string placeholder(std::size_t index) const;
  };

  class PgBuilder : public Builder
  {
  protected:
    [[nodiscard]]
    std::string placeholder(std::size_t index) const override;
  };

  class MySqlBuilder : public Builder
  {};

  class SqliteBuilder : public Builder
  {};

  template <typename T>
  concept SqlBuilder = std::derived_from<std::remove_cvref_t<T>, Builder>;

  [[nodiscard]]
  std::unique_ptr<Builder> getBuilder();

} // namespace worm::core
