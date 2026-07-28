#include <errors/reflection-exception.hpp>
#include <errors/worm-exception.hpp>

#include <iostream>
#include <string>
#include <type_traits>

int main()
{
  static_assert(std::is_base_of_v<worm::WormException, worm::ReflectionException>);

  const std::string expected = "Invalid reflection metadata";
  const worm::ReflectionException error(expected);

  if (std::string(error.what()) != expected) {
    std::cerr << "ReflectionException did not preserve its message.\n";
    return 1;
  }

  try {
    throw worm::ReflectionException(expected);
  } catch (const worm::WormException& caught) {
    return std::string(caught.what()) == expected ? 0 : 1;
  }
}
