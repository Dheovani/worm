#include <errors/migration-exception.hpp>
#include <errors/worm-exception.hpp>

#include <iostream>
#include <string>
#include <type_traits>

int main()
{
  static_assert(std::is_base_of_v<worm::WormException, worm::MigrationException>);

  const worm::MigrationException error("Invalid migration '{}'", "create-users");
  if (std::string{error.what()} != "Invalid migration 'create-users'") {
    std::cerr << "MigrationException did not preserve its formatted message.\n";
    return 1;
  }

  try {
    throw error;
  } catch (const worm::WormException& caught) {
    return std::string{caught.what()} == error.what() ? 0 : 1;
  }
}
