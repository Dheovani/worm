#include <errors/dependency-injection-exception.hpp>
#include <errors/dependency-resolution-exception.hpp>

#include <iostream>
#include <string>
#include <type_traits>

int main()
{
  static_assert(std::is_base_of_v<worm::DependencyInjectionException, worm::DependencyResolutionException>);

  const std::string expected = "Unable to resolve dependency";
  const worm::DependencyResolutionException error(expected);

  if (std::string(error.what()) != expected) {
    std::cerr << "DependencyResolutionException did not preserve its message.\n";
    return 1;
  }

  try {
    throw worm::DependencyResolutionException(expected);
  } catch (const worm::DependencyInjectionException& caught) {
    return std::string(caught.what()) == expected ? 0 : 1;
  }
}
