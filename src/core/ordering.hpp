#pragma once

#include <string_view>

namespace worm::core
{

  enum class OrderDirection
  {
    Ascending,
    Descending
  };

  struct Ordering
  {
    const std::string_view column;
    const OrderDirection direction;

    Ordering(std::string_view column, OrderDirection direction = OrderDirection::Ascending) noexcept
      : column(column),
        direction(direction)
    {}
  };

} // namespace worm::core
