#include <errors/database-exception.hpp>
#include <errors/transaction-exception.hpp>

#include <iostream>
#include <string>
#include <type_traits>

int main()
{
  static_assert(std::is_base_of_v<worm::DatabaseException, worm::TransactionException>);

  const std::string expected = "Unable to finish transaction";
  const worm::TransactionException error(expected);

  if (std::string(error.what()) != expected) {
    std::cerr << "TransactionException did not preserve its message.\n";
    return 1;
  }

  try {
    throw worm::TransactionException(expected);
  } catch (const worm::DatabaseException& caught) {
    return std::string(caught.what()) == expected ? 0 : 1;
  }
}
