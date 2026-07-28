#include <errors/invalid-arg-exception.hpp>
#include <errors/sql-build-exception.hpp>

#include <iostream>
#include <string>
#include <type_traits>

int main()
{
  static_assert(std::is_base_of_v<worm::InvalidArgException, worm::SqlBuildException>);

  const std::string expected = "Unable to build SQL";
  const worm::SqlBuildException error(expected);

  if (std::string(error.what()) != expected) {
    std::cerr << "SqlBuildException did not preserve its message.\n";
    return 1;
  }

  try {
    throw worm::SqlBuildException(expected);
  } catch (const worm::InvalidArgException& caught) {
    return std::string(caught.what()) == expected ? 0 : 1;
  }
}
