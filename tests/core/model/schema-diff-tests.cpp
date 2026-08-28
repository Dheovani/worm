#include <core/model/schema-diff.hpp>

#include <cstdint>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>

namespace
{
  struct User
  {
    std::int64_t id{};
    std::string name;
    std::string email;
    std::string ignoredValue;

    static constexpr worm::core::Table table() noexcept
    {
      return worm::core::Table{"users"};
    }

    static constexpr worm::core::PrimaryKey primaryKey() noexcept
    {
      return worm::core::PrimaryKey{"pk_users", {worm::core::Column{"id", table()}}};
    }

    static constexpr auto reflect() noexcept
    {
      return std::tuple{worm::reflection::field("id", &User::id, {.generated = true, .nullable = false}),
        worm::reflection::field("name", &User::name, {.nullable = false}),
        worm::reflection::field("email", &User::email, {.unique = true, .nullable = false}),
        worm::reflection::field("ignoredValue", &User::ignoredValue, {.ignored = true})};
    }
  };

  worm::core::Column column(std::string_view name, worm::reflection::FieldMetadata metadata = {})
  {
    metadata.columnName = name;
    return worm::core::Column{metadata, User::table()};
  }

  bool hasDifference(
    const std::vector<worm::core::SchemaDifference>& differences,
    worm::core::SchemaDifferenceKind kind,
    std::string_view column = {})
  {
    for (const worm::core::SchemaDifference& difference : differences) {
      if (difference.kind == kind && (column.empty() || difference.column == column)) {
        return true;
      }
    }

    return false;
  }
} // namespace

int main()
{
  const worm::core::TableMetadata matchingTable{User::table(),
    {
      column("id", {.generated = true, .nullable = false}),
      column("name", {.nullable = false}),
      column("email", {.unique = true, .nullable = false}),
    },
    User::primaryKey()};

  if (!worm::core::compareEntityWithTable<User>(matchingTable).empty()) {
    std::cerr << "Schema diff reported differences for a matching table.\n";
    return 1;
  }

  const worm::core::SchemaMetadata matchingSchema{worm::core::Schema{}, {matchingTable}};
  if (!worm::core::compareEntityWithSchema<User>(matchingSchema).empty()) {
    std::cerr << "Schema diff reported differences for a matching schema.\n";
    return 1;
  }

  const worm::core::TableMetadata incompatibleTable{User::table(),
    {
      column("id", {.nullable = false}),
      column("name"),
      column("email", {.nullable = false}),
      column("legacy_name"),
    },
    worm::core::PrimaryKey{"pk_users_email", {worm::core::Column{"email", User::table()}}}};
  const std::vector<worm::core::SchemaDifference> incompatibleDifferences =
    worm::core::compareEntityWithTable<User>(incompatibleTable);

  if (!hasDifference(incompatibleDifferences, worm::core::SchemaDifferenceKind::GeneratedMismatch, "id") ||
      !hasDifference(incompatibleDifferences, worm::core::SchemaDifferenceKind::NullableMismatch, "name") ||
      !hasDifference(incompatibleDifferences, worm::core::SchemaDifferenceKind::UniqueMismatch, "email") ||
      !hasDifference(incompatibleDifferences, worm::core::SchemaDifferenceKind::UnexpectedColumn, "legacy_name") ||
      !hasDifference(incompatibleDifferences, worm::core::SchemaDifferenceKind::PrimaryKeyMismatch)) {
    std::cerr << "Schema diff did not report expected metadata mismatches.\n";
    return 1;
  }

  const worm::core::TableMetadata missingColumnTable{User::table(),
    {
      column("id", {.generated = true, .nullable = false}),
      column("name", {.nullable = false}),
    },
    User::primaryKey()};
  const std::vector<worm::core::SchemaDifference> missingColumnDifferences =
    worm::core::compareEntityWithTable<User>(missingColumnTable);

  if (!hasDifference(missingColumnDifferences, worm::core::SchemaDifferenceKind::MissingColumn, "email")) {
    std::cerr << "Schema diff did not report a missing persistent column.\n";
    return 1;
  }

  const worm::core::TableMetadata missingPrimaryKeyTable{User::table(),
    {
      column("id", {.generated = true, .nullable = false}),
      column("name", {.nullable = false}),
      column("email", {.unique = true, .nullable = false}),
    }};
  const std::vector<worm::core::SchemaDifference> missingPrimaryKeyDifferences =
    worm::core::compareEntityWithTable<User>(missingPrimaryKeyTable);

  if (!hasDifference(missingPrimaryKeyDifferences, worm::core::SchemaDifferenceKind::MissingPrimaryKey)) {
    std::cerr << "Schema diff did not report a missing primary key.\n";
    return 1;
  }

  const worm::core::SchemaMetadata missingTableSchema{worm::core::Schema{}, {}};
  const std::vector<worm::core::SchemaDifference> missingTableDifferences =
    worm::core::compareEntityWithSchema<User>(missingTableSchema);

  if (missingTableDifferences.size() != 1 ||
      missingTableDifferences.front().kind != worm::core::SchemaDifferenceKind::MissingTable ||
      missingTableDifferences.front().table != User::table()) {
    std::cerr << "Schema diff did not report a missing entity table.\n";
    return 1;
  }

  return 0;
}
