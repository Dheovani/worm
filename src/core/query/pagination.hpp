#pragma once

#include <errors/invalid-arg-exception.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>

namespace worm::core
{

  class Pagination final
  {
  public:
    explicit Pagination(std::size_t limit, std::size_t offset = 0)
      : limit_(validateLimit(limit)),
        offset_(validateOffset(offset))
    {}

    [[nodiscard]]
    std::size_t limit() const noexcept
    {
      return limit_;
    }

    [[nodiscard]]
    std::size_t offset() const noexcept
    {
      return offset_;
    }

  private:
    [[nodiscard]]
    static std::size_t validateLimit(std::size_t limit)
    {
      if (limit == 0 || limit > static_cast<std::size_t>((std::numeric_limits<std::int64_t>::max)())) {
        throw InvalidArgException("Pagination limit must be between 1 and INT64_MAX.");
      }

      return limit;
    }

    [[nodiscard]]
    static std::size_t validateOffset(std::size_t offset)
    {
      if (offset > static_cast<std::size_t>((std::numeric_limits<std::int64_t>::max)())) {
        throw InvalidArgException("Pagination offset must not exceed INT64_MAX.");
      }

      return offset;
    }

    const std::size_t limit_;
    const std::size_t offset_;
  };

} // namespace worm::core
