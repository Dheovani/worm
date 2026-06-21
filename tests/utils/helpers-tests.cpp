#include <utils/helpers.hpp>

#include <errors/invalid-arg-exception.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <type_traits>
#include <variant>

namespace
{
  struct Base
  {};
  struct Derived : Base
  {
    int value = 0;
    int getValue() const
    {
      return value;
    }
  };
} // namespace

int main()
{
  namespace utils = worm::utils;

  static_assert(utils::is_string<std::string>);
  static_assert(utils::is_string<std::string_view>);
  static_assert(utils::is_string<const char*>);
  static_assert(!utils::is_string<int>);
  static_assert(utils::instance_of<Base, Derived>);
  static_assert(utils::instance_of<Base, Derived*>);
  static_assert(utils::is_attribute<decltype(&Derived::value)>);
  static_assert(utils::is_function<decltype(&Derived::getValue)>);
  static_assert(utils::get_variant_index<int, std::variant<std::string, int>> == 1);
  static_assert(utils::holds_variant_option<std::string, std::variant<int, std::string>>);
  static_assert(!utils::holds_variant_option<double, std::variant<int, std::string>>);
  static_assert(
      std::is_same_v<utils::remove_class_pointer_t<decltype(&Derived::value), Derived>, int>);

  const std::filesystem::path originalPath = std::filesystem::current_path();
  const std::filesystem::path root = std::filesystem::temp_directory_path() / "worm-helpers-tests";
  const std::filesystem::path nested = root / "nested";

  std::filesystem::remove_all(root);
  std::filesystem::create_directories(nested);

  {
    std::ofstream envFile(root / ".env");
    envFile << "# ignored comment\n"
            << " database_type = \"sqlite\" \n"
            << " host = localhost\n";
  }

  std::filesystem::current_path(nested);

  int result = 0;
  try {
    const auto variables = utils::env::loadFromPath((root / ".env").string());
    if (variables.at("database_type") != "sqlite" || variables.at("host") != "localhost" ||
        utils::env::findInProjectRoot() != (root / ".env").string() ||
        utils::env::getDatabaseType() != "sqlite") {
      std::cerr << "Environment helpers returned unexpected values.\n";
      result = 1;
    }

    try {
      static_cast<void>(utils::env::loadFromPath((root / "missing.env").string()));
      std::cerr << "loadFromPath accepted a missing file.\n";
      result = 1;
    } catch (const worm::InvalidArgException&) {}
  } catch (const std::exception& error) {
    std::cerr << "Environment helper test failed: " << error.what() << '\n';
    result = 1;
  }

  std::filesystem::current_path(originalPath);
  std::filesystem::remove_all(root);
  return result;
}
