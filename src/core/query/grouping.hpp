#pragma once

#include <string_view>

namespace worm::core
{

  struct Grouping
  {
    const std::string_view column;

    explicit Grouping(std::string_view column) noexcept
      : column(column)
    {}
  };

} // namespace worm::core
