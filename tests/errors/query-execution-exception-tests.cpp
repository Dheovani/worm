#include <errors/database-exception.hpp>
#include <errors/query-execution-exception.hpp>

#include <iostream>
#include <string>
#include <type_traits>

int main()
{
  static_assert(std::is_base_of_v<worm::DatabaseException, worm::QueryExecutionException>);

  const std::string expected = "Unable to execute query";
  const worm::QueryExecutionException error(expected);

  if (std::string(error.what()) != expected) {
    std::cerr << "QueryExecutionException did not preserve its message.\n";
    return 1;
  }

  try {
    throw worm::QueryExecutionException(expected);
  } catch (const worm::DatabaseException& caught) {
    return std::string(caught.what()) == expected ? 0 : 1;
  }
}
