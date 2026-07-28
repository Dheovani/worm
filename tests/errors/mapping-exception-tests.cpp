#include <errors/mapping-exception.hpp>
#include <errors/worm-exception.hpp>

#include <iostream>
#include <string>
#include <type_traits>

int main()
{
  static_assert(std::is_base_of_v<worm::WormException, worm::MappingException>);

  const std::string expected = "Invalid ORM mapping";
  const worm::MappingException error(expected);

  if (std::string(error.what()) != expected) {
    std::cerr << "MappingException did not preserve its message.\n";
    return 1;
  }

  try {
    throw worm::MappingException(expected);
  } catch (const worm::WormException& caught) {
    return std::string(caught.what()) == expected ? 0 : 1;
  }
}
