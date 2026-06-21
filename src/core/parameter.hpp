#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>

namespace worm
{
  namespace core
  {
    using Parameter = std::variant<std::nullptr_t, std::int64_t, double, bool, std::string>;
  } // namespace core
} // namespace worm
