#include <errors/database-connection-exception.hpp>
#include <errors/database-exception.hpp>

#include <iostream>
#include <string>
#include <type_traits>

int main()
{
  static_assert(std::is_base_of_v<worm::DatabaseException, worm::DatabaseConnectionException>);

  const std::string expected = "Unable to connect to database";
  const worm::DatabaseConnectionException error(expected);

  if (std::string(error.what()) != expected) {
    std::cerr << "DatabaseConnectionException did not preserve its message.\n";
    return 1;
  }

  try {
    throw worm::DatabaseConnectionException(expected);
  } catch (const worm::DatabaseException& caught) {
    return std::string(caught.what()) == expected ? 0 : 1;
  }
}
