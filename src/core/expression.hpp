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

} // namespace worm::core
