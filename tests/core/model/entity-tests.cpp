#include <core/model/entity-metadata.hpp>

#include <string>
#include <tuple>
#include <type_traits>

namespace
{
  struct User
  {
    int id = 0;
    std::string name;
    int transientValue = 0;

    [[nodiscard]]
    static constexpr worm::core::Table table() noexcept
    {
      return worm::core::Table{"users"};
    }

    [[nodiscard]]
    static constexpr auto reflect()
    {
      return std::tuple{
        worm::reflection::field("id", &User::id, {.primaryKey = true}),
        worm::reflection::field("name", &User::name),
        worm::reflection::field("transientValue", &User::transientValue, {.ignored = true})};
    }
  };

  struct EmptyTable
  {
    int id = 0;

    [[nodiscard]]
    static constexpr worm::core::Table table() noexcept
    {
      return worm::core::Table{""};
    }

    [[nodiscard]]
    static constexpr auto reflect()
    {
      return std::tuple{worm::reflection::field("id", &EmptyTable::id, {.primaryKey = true})};
    }
  };

  struct MissingPrimaryKey
  {
    int id = 0;

    [[nodiscard]]
    static constexpr worm::core::Table table() noexcept
    {
      return worm::core::Table{"missing_primary_keys"};
    }

    [[nodiscard]]
    static constexpr auto reflect()
    {
      return std::tuple{worm::reflection::field("id", &MissingPrimaryKey::id)};
    }
  };

  struct DuplicatedPrimaryKey
  {
    int id = 0;
    int externalId = 0;

    [[nodiscard]]
    static constexpr worm::core::Table table() noexcept
    {
      return worm::core::Table{"duplicated_primary_keys"};
    }

    [[nodiscard]]
    static constexpr auto reflect()
    {
      return std::tuple{
        worm::reflection::field("id", &DuplicatedPrimaryKey::id, {.primaryKey = true}),
        worm::reflection::field("externalId", &DuplicatedPrimaryKey::externalId, {.primaryKey = true})};
    }
  };

  struct IgnoredPrimaryKey
  {
    int id = 0;
    std::string name;

    [[nodiscard]]
    static constexpr worm::core::Table table() noexcept
    {
      return worm::core::Table{"ignored_primary_keys"};
    }

    [[nodiscard]]
    static constexpr auto reflect()
    {
      return std::tuple{
        worm::reflection::field("id", &IgnoredPrimaryKey::id, {.primaryKey = true, .ignored = true}),
        worm::reflection::field("name", &IgnoredPrimaryKey::name)};
    }
  };

  struct MissingTable
  {
    int id = 0;

    [[nodiscard]]
    static constexpr auto reflect()
    {
      return std::tuple{worm::reflection::field("id", &MissingTable::id)};
    }
  };

  struct InvalidTableReturn
  {
    int id = 0;

    [[nodiscard]]
    static constexpr std::string_view table() noexcept
    {
      return "invalid";
    }

    [[nodiscard]]
    static constexpr auto reflect()
    {
      return std::tuple{worm::reflection::field("id", &InvalidTableReturn::id)};
    }
  };
} // namespace

int main()
{
  constexpr worm::core::Table users{"users"};
  constexpr worm::core::Table sameUsers{"users"};
  constexpr worm::core::Table orders{"orders"};

  static_assert(users.name() == "users");
  static_assert(!users.empty());
  static_assert(users == sameUsers);
  static_assert(!(users == orders));
  static_assert(std::is_empty_v<decltype(users)> == false);
  static_assert(worm::core::EntityState::Transient != worm::core::EntityState::Managed);

  static_assert(worm::core::Entity<User>);
  static_assert(worm::core::Entity<const User&>);
  static_assert(worm::core::Entity<MissingPrimaryKey>);
  static_assert(!worm::core::Entity<MissingTable>);
  static_assert(!worm::core::Entity<InvalidTableReturn>);
  static_assert(User::table() == users);
  static_assert(worm::core::table_of<User>() == users);
  static_assert(worm::core::has_valid_table<User>());
  static_assert(!worm::core::has_valid_table<EmptyTable>());
  static_assert(std::tuple_size_v<decltype(worm::core::fields_of<User>())> == 3);
  static_assert(worm::core::persistent_field_count<User> == 2);
  static_assert(worm::core::primary_key_count<User> == 1);
  static_assert(worm::core::has_single_primary_key<User>);
  static_assert(worm::core::has_primary_key<User>());
  static_assert(worm::core::is_valid_entity<User>());
  static_assert(worm::core::PersistableEntity<User>);
  static_assert(worm::core::primary_key_field_of<User>().name() == "id");
  static_assert(!worm::core::is_valid_entity<EmptyTable>());
  static_assert(!worm::core::is_valid_entity<MissingPrimaryKey>());
  static_assert(!worm::core::is_valid_entity<DuplicatedPrimaryKey>());
  static_assert(!worm::core::is_valid_entity<IgnoredPrimaryKey>());
  static_assert(!worm::core::PersistableEntity<EmptyTable>);
  static_assert(!worm::core::PersistableEntity<MissingPrimaryKey>);
  static_assert(!worm::core::PersistableEntity<DuplicatedPrimaryKey>);
  static_assert(!worm::core::PersistableEntity<IgnoredPrimaryKey>);
  static_assert(worm::core::primary_key_count<IgnoredPrimaryKey> == 0);

  return 0;
}
