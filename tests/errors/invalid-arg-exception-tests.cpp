#include <errors/invalid-arg-exception.hpp>

#include <exception>
#include <iostream>
#include <string>
#include <type_traits>

int main()
{
  static_assert(std::is_base_of_v<std::exception, worm::InvalidArgException>);

  const std::string expected = "A meaningful invalid argument error";
  const worm::InvalidArgException error(expected);

  if (std::string(error.what()) != expected) {
    std::cerr << "InvalidArgException did not preserve its message.\n";
    return 1;
  }

  try {
    throw worm::InvalidArgException(expected);
  } catch (const std::exception& caught) {
    if (std::string(caught.what()) == expected)
      return 0;

    std::cerr << "InvalidArgException changed its message when caught polymorphically.\n";
    return 1;
  } catch (...) {
    std::cerr << "InvalidArgException was not caught as std::exception.\n";
    return 1;
  }
}
