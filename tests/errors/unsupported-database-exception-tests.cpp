#include <errors/configuration-exception.hpp>
#include <errors/unsupported-database-exception.hpp>

#include <iostream>
#include <string>
#include <type_traits>

int main()
{
  static_assert(std::is_base_of_v<worm::ConfigurationException, worm::UnsupportedDatabaseException>);

  const std::string expected = "Unsupported database type";
  const worm::UnsupportedDatabaseException error(expected);

  if (std::string(error.what()) != expected) {
    std::cerr << "UnsupportedDatabaseException did not preserve its message.\n";
    return 1;
  }

  try {
    throw worm::UnsupportedDatabaseException(expected);
  } catch (const worm::ConfigurationException& caught) {
    return std::string(caught.what()) == expected ? 0 : 1;
  }
}
