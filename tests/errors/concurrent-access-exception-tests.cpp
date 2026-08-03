#include <errors/concurrent-access-exception.hpp>
#include <errors/worm-exception.hpp>

#include <iostream>
#include <string>
#include <type_traits>

int main()
{
  static_assert(std::is_base_of_v<worm::WormException, worm::ConcurrentAccessException>);

  const std::string expected = "Concurrent access";
  const worm::ConcurrentAccessException error(expected);
  if (error.what() != expected) {
    std::cerr << "ConcurrentAccessException did not preserve its message.\n";
    return 1;
  }

  return 0;
}
