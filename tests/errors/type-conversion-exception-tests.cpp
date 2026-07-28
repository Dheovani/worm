#include <errors/mapping-exception.hpp>
#include <errors/type-conversion-exception.hpp>

#include <iostream>
#include <string>
#include <type_traits>

int main()
{
  static_assert(std::is_base_of_v<worm::MappingException, worm::TypeConversionException>);

  const std::string expected = "Unable to convert SQL value";
  const worm::TypeConversionException error(expected);

  if (std::string(error.what()) != expected) {
    std::cerr << "TypeConversionException did not preserve its message.\n";
    return 1;
  }

  try {
    throw worm::TypeConversionException(expected);
  } catch (const worm::MappingException& caught) {
    return std::string(caught.what()) == expected ? 0 : 1;
  }
}
