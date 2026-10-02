#include <reflection/field.hpp>
#include <reflection/lookup.hpp>
#include <reflection/snapshot.hpp>
#include <reflection/visit.hpp>

#include <concepts>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>

namespace
{
  struct User
  {
    std::int64_t id{};
    std::string name;
    std::optional<std::string> email;
    std::string displayState;

    static constexpr auto reflect() noexcept
    {
      return std::tuple{worm::reflection::field("id", &User::id, {.nullable = false}),
        worm::reflection::field("name", &User::name, {.columnName = "full_name", .nullable = false}),
        worm::reflection::field("email", &User::email),
        worm::reflection::field("displayState", &User::displayState, {.ignored = true})};
    }
  };
} // namespace

int main()
{
  static_assert(worm::reflection::field_count<User> == 4);
  static_assert(worm::reflection::find_field_index<User>("name") == 1);
  static_assert(worm::reflection::find_column_index<User>("full_name") == 1);

  User user{.id = 1, .name = "Ada", .email = "ada@example.com", .displayState = "visible"};
  const auto snapshot = worm::reflection::make_snapshot(user);

  worm::reflection::for_each_field(user, [](const auto& descriptor, const auto& value) {
    std::cout << descriptor.name() << " -> " << descriptor.columnName();
    if (descriptor.isIgnored()) {
      std::cout << " (ignored)";
    }
    std::cout << "\n";
    static_cast<void>(value);
  });

  const bool nameVisited = worm::reflection::visit_field(user, "name", [](const auto&, auto& value) {
    using Value = std::remove_cvref_t<decltype(value)>;
    if constexpr (std::same_as<Value, std::string>) {
      value = "Ada Lovelace";
    }
  });
  user.displayState = "hidden";

  const std::size_t changed =
    worm::reflection::for_each_changed_field(user, snapshot, [](const auto& descriptor, const auto&, const auto&) {
      std::cout << "changed: " << descriptor.name() << "\n";
    });

  return nameVisited && changed == 1 && worm::reflection::is_dirty(user, snapshot) ? 0 : 1;
}
