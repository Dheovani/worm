#include <errors/configuration-exception.hpp>
#include <errors/worm-exception.hpp>

#include <iostream>
#include <string>
#include <type_traits>

int main()
{
  static_assert(std::is_base_of_v<worm::WormException, worm::ConfigurationException>);

  const std::string expected = "Invalid configuration";
  const worm::ConfigurationException error(expected);

  if (std::string(error.what()) != expected) {
    std::cerr << "ConfigurationException did not preserve its message.\n";
    return 1;
  }

  try {
    throw worm::ConfigurationException(expected);
  } catch (const worm::WormException& caught) {
    return std::string(caught.what()) == expected ? 0 : 1;
  }
}
