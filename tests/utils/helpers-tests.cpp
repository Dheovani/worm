#include <utils/helpers.hpp>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>

namespace
{
  constexpr const char* environmentKey = "WORM_HELPERS_TEST_VALUE";

  void setEnvironment(const char* key, const char* value)
  {
#ifdef _WIN32
    _putenv_s(key, value);
#else
    setenv(key, value, 1);
#endif
  }

  void unsetEnvironment(const char* key)
  {
#ifdef _WIN32
    _putenv_s(key, "");
#else
    unsetenv(key);
#endif
  }

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

  static_assert(utils::is_string_like<std::string>);
  static_assert(utils::is_string_like<std::string_view>);
  static_assert(utils::is_string_like<const char*>);
  static_assert(!utils::is_string_like<int>);
  static_assert(utils::is_optional<std::optional<int>>::value);
  static_assert(utils::is_optional_v<std::optional<int>>);
  static_assert(utils::is_optional_v<const std::optional<std::string>&>);
  static_assert(!utils::is_optional_v<std::string>);
  static_assert(std::is_same_v<utils::remove_optional_t<std::optional<int>>, int>);
  static_assert(std::is_same_v<utils::remove_optional_t<const std::optional<std::string>&>, std::string>);
  static_assert(utils::is_date_type<std::chrono::sys_days>);
  static_assert(utils::is_date_type<const std::chrono::sys_days&>);
  static_assert(!utils::is_date_type<std::chrono::system_clock::time_point>);
  static_assert(!utils::is_date_type<std::string>);
  static_assert(utils::instance_of<Base, Derived>);
  static_assert(utils::instance_of<Base, Derived*>);
  static_assert(utils::is_attribute<decltype(&Derived::value)>);
  static_assert(utils::is_function<decltype(&Derived::getValue)>);
  static_assert(utils::get_variant_index<int, std::variant<std::string, int>> == 1);
  static_assert(utils::holds_variant_option<std::string, std::variant<int, std::string>>);
  static_assert(!utils::holds_variant_option<double, std::variant<int, std::string>>);
  static_assert(std::is_same_v<utils::remove_class_pointer_t<decltype(&Derived::value), Derived>, int>);

  setEnvironment(environmentKey, "configured");
  if (utils::env::envValue(environmentKey) != "configured") {
    std::cerr << "envValue did not return the configured environment value.\n";
    unsetEnvironment(environmentKey);
    return 1;
  }

  unsetEnvironment(environmentKey);
  if (!utils::env::envValue(environmentKey).empty()) {
    std::cerr << "envValue returned a value for a missing environment variable.\n";
    return 1;
  }

  return 0;
}
