#include <core/model/entity-metadata.hpp>
#include <reflection/field.hpp>

#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <tuple>

namespace
{
  struct Role
  {
    std::int64_t id{};

    static constexpr worm::core::Table table() noexcept
    {
      return worm::core::Table{worm::core::Schema{"public"}, "roles"};
    }

    static constexpr worm::core::PrimaryKey primaryKey() noexcept
    {
      return worm::core::PrimaryKey{"pk_roles", {worm::core::Column{"id", table()}}};
    }

    static constexpr auto reflect() noexcept
    {
      return std::tuple{worm::reflection::field("id", &Role::id, {.generated = true, .nullable = false})};
    }
  };

  struct User
  {
    std::int64_t id{};
    std::int64_t roleId{};
    std::optional<std::string> email;
    std::string settings;
    std::string transient;

    static constexpr worm::core::Table table() noexcept
    {
      return worm::core::Table{worm::core::Schema{"public"}, "users"};
    }

    static constexpr worm::core::PrimaryKey primaryKey() noexcept
    {
      return worm::core::PrimaryKey{"pk_users", {worm::core::Column{"id", table()}}};
    }

    static constexpr auto reflect() noexcept
    {
      return std::tuple{worm::reflection::field("id", &User::id, {.generated = true, .nullable = false}),
        worm::reflection::field("roleId", &User::roleId, {.columnName = "role_id", .nullable = false}),
        worm::reflection::field("email", &User::email, {.unique = true}),
        worm::reflection::field("settings", &User::settings, {.nullable = false}),
        worm::reflection::field("transient", &User::transient, {.ignored = true})};
    }

    static worm::core::ColumnType columnType(std::string_view column)
    {
      if (column == "settings") {
        return {.kind = worm::core::ColumnTypeKind::Json};
      }
      if (column == "email") {
        return {.kind = worm::core::ColumnTypeKind::String, .length = 320};
      }
      return {};
    }

    static constexpr auto indexes() noexcept
    {
      return std::tuple{worm::core::Index{"idx_users_email", {{worm::core::Column{"email", table()}}}, true}};
    }

    static constexpr auto foreignKeys() noexcept
    {
      return std::tuple{worm::core::ForeignKey{"fk_users_role",
        {worm::core::Column{"role_id", table()}},
        Role::table(),
        {worm::core::Column{"id", Role::table()}},
        {{worm::core::Operation::Delete, worm::core::ReferentialAction::Restrict}}}};
    }
  };
} // namespace

int main()
{
  static_assert(worm::core::MappableColumn<std::int64_t>);
  static_assert(worm::core::MappableColumn<std::optional<std::string>>);
  static_assert(!worm::core::MappableColumn<std::uint64_t>);
  static_assert(!worm::core::MappableColumn<long double>);
  static_assert(!worm::core::MappableColumn<std::tuple<int>>);

  const worm::core::ColumnType integer = worm::core::column_type_of<std::int64_t>();
  const worm::core::ColumnType text = worm::core::column_type_of<std::optional<std::string>>();
  if (integer.kind != worm::core::ColumnTypeKind::Int64 || text.kind != worm::core::ColumnTypeKind::String) {
    std::cerr << "C++ column type inference failed.\n";
    return 1;
  }

  const worm::core::TableMetadata users = worm::core::table_metadata_of<User>();
  const auto* id = users.findColumn("id");
  const auto* email = users.findColumn("email");
  const auto* settings = users.findColumn("settings");
  if (users.table() != User::table() || users.columns().size() != 4 || id == nullptr || email == nullptr ||
      settings == nullptr || id->type().kind != worm::core::ColumnTypeKind::Int64 || !id->generated ||
      email->type().kind != worm::core::ColumnTypeKind::String || email->type().length != 320 || !email->unique ||
      settings->type().kind != worm::core::ColumnTypeKind::Json || users.indexes().size() != 1 ||
      users.foreignKeys().size() != 1 ||
      users.foreignKeys().front().referentialActionFor(worm::core::Operation::Delete) !=
        worm::core::ReferentialAction::Restrict) {
    std::cerr << "Reflected entity metadata conversion failed.\n";
    return 1;
  }

  const worm::core::SchemaMetadata schema = worm::core::schema_metadata_of<Role, User>();
  if (schema.tables().size() != 2 || schema.findTable(Role::table()) == nullptr ||
      schema.findTable(User::table()) == nullptr) {
    std::cerr << "Reflected schema metadata conversion failed.\n";
    return 1;
  }

  return 0;
}
