#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <core/parameter.hpp>

namespace worm
{
  namespace core
  {
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

    class Expressions final
    {
    public:
      [[nodiscard]] static Expression compare(std::string_view column, Comparison comparison,
                                              Parameter value);

      [[nodiscard]] static Expression isNull(std::string_view column);
      [[nodiscard]] static Expression isNotNull(std::string_view column);

      [[nodiscard]] static Expression between(std::string_view column, Parameter lower,
                                              Parameter upper);

      [[nodiscard]] static Expression in(std::string_view column, std::vector<Parameter> values);

      [[nodiscard]] static Expression notIn(std::string_view column, std::vector<Parameter> values);
    };
  } // namespace core
} // namespace worm
