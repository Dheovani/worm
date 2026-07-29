#include <errors/dependency-injection-exception.hpp>
#include <errors/worm-exception.hpp>

#include <iostream>
#include <string>
#include <type_traits>

int main()
{
  static_assert(std::is_base_of_v<worm::WormException, worm::DependencyInjectionException>);

  const std::string expected = "Dependency injection failed";
  const worm::DependencyInjectionException error(expected);

  if (std::string(error.what()) != expected) {
    std::cerr << "DependencyInjectionException did not preserve its message.\n";
    return 1;
  }

  try {
    throw worm::DependencyInjectionException(expected);
  } catch (const worm::WormException& caught) {
    return std::string(caught.what()) == expected ? 0 : 1;
  }
}
