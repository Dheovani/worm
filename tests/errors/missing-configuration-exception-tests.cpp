#include <errors/configuration-exception.hpp>
#include <errors/missing-configuration-exception.hpp>

#include <iostream>
#include <string>
#include <type_traits>

int main()
{
  static_assert(std::is_base_of_v<worm::ConfigurationException, worm::MissingConfigurationException>);

  const std::string expected = "Missing configuration value";
  const worm::MissingConfigurationException error(expected);

  if (std::string(error.what()) != expected) {
    std::cerr << "MissingConfigurationException did not preserve its message.\n";
    return 1;
  }

  try {
    throw worm::MissingConfigurationException(expected);
  } catch (const worm::ConfigurationException& caught) {
    return std::string(caught.what()) == expected ? 0 : 1;
  }
}
