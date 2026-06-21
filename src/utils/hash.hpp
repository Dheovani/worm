#pragma once

#include <cstdint>
#include <string_view>

namespace worm
{
  using Hash = std::uint64_t;

  [[nodiscard]] constexpr Hash hashCode(std::string_view key) noexcept
  {
    Hash value = 0;
    for (const char character : key)
      value = value * 37 + static_cast<unsigned char>(character);

    return value;
  }
} // namespace worm
