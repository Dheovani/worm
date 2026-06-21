#include <errors/database-exception.hpp>

#include <exception>
#include <iostream>
#include <string>
#include <type_traits>

int main()
{
  static_assert(std::is_base_of_v<std::exception, worm::DatabaseException>);

  const std::string expected = "A meaningful database error";
  const worm::DatabaseException error(expected);

  if (std::string(error.what()) != expected) {
    std::cerr << "DatabaseException did not preserve its message.\n";
    return 1;
  }

  try {
    throw worm::DatabaseException(expected);
  } catch (const std::exception& caught) {
    if (std::string(caught.what()) == expected)
      return 0;

    std::cerr << "DatabaseException changed its message when caught polymorphically.\n";
    return 1;
  } catch (...) {
    std::cerr << "DatabaseException was not caught as std::exception.\n";
    return 1;
  }
}
