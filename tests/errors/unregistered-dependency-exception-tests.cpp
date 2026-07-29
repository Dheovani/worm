#include <errors/dependency-resolution-exception.hpp>
#include <errors/unregistered-dependency-exception.hpp>

#include <iostream>
#include <string>
#include <type_traits>

int main()
{
  static_assert(std::is_base_of_v<worm::DependencyResolutionException, worm::UnregisteredDependencyException>);

  const std::string expected = "Dependency is not registered";
  const worm::UnregisteredDependencyException error(expected);

  if (std::string(error.what()) != expected) {
    std::cerr << "UnregisteredDependencyException did not preserve its message.\n";
    return 1;
  }

  try {
    throw worm::UnregisteredDependencyException(expected);
  } catch (const worm::DependencyResolutionException& caught) {
    return std::string(caught.what()) == expected ? 0 : 1;
  }
}
