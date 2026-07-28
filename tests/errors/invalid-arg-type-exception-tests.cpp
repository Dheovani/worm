#include <errors/invalid-arg-exception.hpp>
#include <errors/invalid-arg-type-exception.hpp>

#include <iostream>
#include <string>
#include <type_traits>

int main()
{
  static_assert(std::is_base_of_v<worm::InvalidArgException, worm::InvalidArgTypeException>);

  const std::string expected = "Invalid argument type";
  const worm::InvalidArgTypeException error(expected);

  if (std::string(error.what()) != expected) {
    std::cerr << "InvalidArgTypeException did not preserve its message.\n";
    return 1;
  }

  try {
    throw worm::InvalidArgTypeException(expected);
  } catch (const worm::InvalidArgException& caught) {
    return std::string(caught.what()) == expected ? 0 : 1;
  }
}
