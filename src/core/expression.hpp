#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace worm::core
{

	using Parameter = std::variant<std::nullptr_t, std::int64_t, double, bool, std::string>;

  enum class Comparison
  {
    Equal,
    NotEqual,
    Greater,
    GreaterOrEqual,
    Less,
    LessOrEqual,
    Like
  };

  struct Expression
  {
    std::string sql;
    std::vector<Parameter> parameters;
  };

  class ExpressionBuilder final
  {
  public:
    [[nodiscard]]
		static Expression compare(std::string_view column, Comparison comparison, Parameter value);

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
    static Expression _and(Expression& left, Expression& right);

		[[nodiscard]]
    static Expression _or(Expression& left, Expression& right);
  };

} // namespace worm::core
