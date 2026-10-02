#include <core/model/entity-metadata.hpp>
#include <core/model/migration.hpp>
#include <core/model/schema-diff.hpp>
#include <core/model/schema-snapshot.hpp>
#include <core/query/migration-ddl.hpp>
#include <core/query/sql-builder.hpp>
#include <reflection/field.hpp>

#include <cstdint>
#include <iostream>
#include <string>
#include <tuple>

namespace
{
  struct User
  {
    std::int64_t id{};
    std::string email;

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
      return std::tuple{worm::reflection::field("id", &User::id, {.nullable = false}),
        worm::reflection::field("email", &User::email, {.unique = true, .nullable = false})};
    }

    static constexpr auto indexes() noexcept
    {
      return std::tuple{worm::core::Index{"idx_users_email", {{worm::core::Column{"email", table()}}}, true}};
    }
  };
} // namespace

int main()
{
  const worm::core::SchemaMetadata expected = worm::core::schema_metadata_of<User>();
  const worm::core::SchemaSnapshot current{};
  const auto differences = worm::core::compareSchemas(expected, current);
  const worm::core::MigrationPlan plan = worm::core::generateMigrationPlan(differences);
  const worm::core::MigrationDdlPlan ddl =
    worm::core::compileMigrationDdl(plan, expected, current, worm::core::SqliteBuilder{});

  std::cout << "differences: " << differences.size() << "\n";
  for (const auto& step : ddl.steps()) {
    std::cout << step.description << "\n";
    for (const auto& statement : step.statements) {
      std::cout << "  " << statement.sql << "\n";
    }
  }

  return differences.size() == 1 && !plan.empty() && !ddl.statements().empty() ? 0 : 1;
}
