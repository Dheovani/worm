#include <errors/worm-exception.hpp>

#include <exception>
#include <iostream>
#include <string>
#include <type_traits>

int main()
{
  static_assert(std::is_base_of_v<std::exception, worm::WormException>);

  const std::string expected = "A meaningful Worm error";
  const worm::WormException error(expected);

  if (std::string(error.what()) != expected) {
    std::cerr << "WormException did not preserve its message.\n";
    return 1;
  }

  try {
    throw worm::WormException(expected);
  } catch (const std::exception& caught) {
    if (std::string(caught.what()) == expected)
      return 0;

    std::cerr << "WormException changed its message when caught polymorphically.\n";
    return 1;
  } catch (...) {
    std::cerr << "WormException was not caught as std::exception.\n";
    return 1;
  }
}
