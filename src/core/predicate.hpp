#pragma once

#include <core/expression.hpp>

#include <string_view>
#include <vector>

namespace worm::core
{

  class Predicate final
  {
  public:
    [[nodiscard]]
    static Expression compare(std::string_view column, Comparison comparison, Parameter value);

    [[nodiscard]]
    static Expression equal(std::string_view column, Parameter value);

    [[nodiscard]]
    static Expression isNull(std::string_view column);

    [[nodiscard]]
    static Expression isNotNull(std::string_view column);

    [[nodiscard]]
    static Expression between(std::string_view column, Parameter lower, Parameter upper);

    [[nodiscard]]
    static Expression in(std::string_view column, std::vector<Parameter> values);

    [[nodiscard]]
    static Expression notIn(std::string_view column, std::vector<Parameter> values);

    [[nodiscard]]
    static Expression not_(const Expression& expression);

    [[nodiscard]]
    static Expression all(const Expression& left, const Expression& right);

    [[nodiscard]]
    static Expression all(const std::vector<Expression>& expressions);

    [[nodiscard]]
    static Expression any(const Expression& left, const Expression& right);

    [[nodiscard]]
    static Expression any(const std::vector<Expression>& expressions);
  };

} // namespace worm::core
