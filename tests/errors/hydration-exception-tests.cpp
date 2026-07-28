#include <errors/hydration-exception.hpp>
#include <errors/mapping-exception.hpp>

#include <iostream>
#include <string>
#include <type_traits>

int main()
{
  static_assert(std::is_base_of_v<worm::MappingException, worm::HydrationException>);

  const std::string expected = "Unable to hydrate entity";
  const worm::HydrationException error(expected);

  if (std::string(error.what()) != expected) {
    std::cerr << "HydrationException did not preserve its message.\n";
    return 1;
  }

  try {
    throw worm::HydrationException(expected);
  } catch (const worm::MappingException& caught) {
    return std::string(caught.what()) == expected ? 0 : 1;
  }
}
