#include <errors/migration-lock-exception.hpp>

#include <iostream>
#include <string>
#include <type_traits>

int main()
{
  static_assert(std::is_base_of_v<worm::MigrationException, worm::MigrationLockException>);

  const std::string expected = "Could not acquire migration lock 'worm:migrations'.";
  const worm::MigrationLockException error("Could not acquire migration lock '{}'.", "worm:migrations");
  if (error.what() != expected) {
    std::cerr << "MigrationLockException did not preserve its formatted message.\n";
    return 1;
  }

  return 0;
}
