#include <reflection/lookup.hpp>
#include <reflection/metadata.hpp>

#include <iostream>
#include <string>
#include <tuple>
#include <type_traits>

namespace
{
  struct Entity
  {
    int id = 7;
    std::string email = "old@example.com";
    std::string name = "Worm";

    static constexpr auto reflect()
    {
      return std::tuple{
          worm::reflection::field("id", &Entity::id),
          worm::reflection::field("email", &Entity::email,
                                  worm::reflection::FieldMetadata{
                                      .columnName = "email_address",
                                  }),
          worm::reflection::field("name", &Entity::name),
      };
    }
  };

  struct ConstantHasher
  {
    constexpr std::size_t operator()(std::string_view) const noexcept
    {
      return 1;
    }
  };

  static_assert(worm::reflection::find_field_index<Entity>("id") == 0);
  static_assert(worm::reflection::find_field_index<Entity>("email") == 1);
  static_assert(!worm::reflection::find_field_index<Entity>("missing"));
  static_assert(worm::reflection::find_column_index<Entity>("email_address") == 1);
  static_assert(!worm::reflection::find_column_index<Entity>("email"));

  static_assert(worm::reflection::find_field_index<Entity>("name", ConstantHasher{}) == 2);
  static_assert(!worm::reflection::find_field_index<Entity>("missing", ConstantHasher{}));
} // namespace

int main()
{
  Entity entity;
  const bool found = worm::reflection::visit_field(
      entity, "email",
      [](const auto&, auto& value) {
        using Value = std::remove_cvref_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, std::string>)
          value = "new@example.com";
      },
      ConstantHasher{});

  if (!found || entity.email != "new@example.com" || entity.name != "Worm") {
    std::cerr << "Field lookup visited the wrong value during a hash collision.\n";
    return 1;
  }

  std::string columnName;
  const bool columnFound = worm::reflection::visit_column_descriptor<Entity>(
      "email_address",
      [&columnName](const auto& descriptor) { columnName = descriptor.columnName(); });

  if (!columnFound || columnName != "email_address") {
    std::cerr << "Column lookup did not return the expected descriptor.\n";
    return 1;
  }

  bool visitedMissingField = false;
  const bool missingFound = worm::reflection::visit_field_descriptor<Entity>(
      "missing", [&visitedMissingField](const auto&) { visitedMissingField = true; },
      ConstantHasher{});

  if (missingFound || visitedMissingField) {
    std::cerr << "Field lookup accepted a hash collision as an exact match.\n";
    return 1;
  }

  return 0;
}
