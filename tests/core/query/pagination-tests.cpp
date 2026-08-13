#include <core/query/pagination.hpp>

#include <errors/invalid-arg-exception.hpp>

#include <cstddef>
#include <iostream>
#include <limits>

int main()
{
  const worm::core::Pagination pagination{25, 50};
  if (pagination.limit() != 25 || pagination.offset() != 50) {
    std::cerr << "Pagination did not preserve its limit and offset.\n";
    return 1;
  }

  try {
    static_cast<void>(worm::core::Pagination{0});
    std::cerr << "Pagination accepted a zero limit.\n";
    return 1;
  } catch (const worm::InvalidArgException&) {}

  if constexpr ((std::numeric_limits<std::size_t>::max)() >
                static_cast<std::size_t>((std::numeric_limits<std::int64_t>::max)())) {
    try {
      static_cast<void>(worm::core::Pagination{1, (std::numeric_limits<std::size_t>::max)()});
      std::cerr << "Pagination accepted an offset that cannot be represented as a SQL parameter.\n";
      return 1;
    } catch (const worm::InvalidArgException&) {}
  }

  return 0;
}
