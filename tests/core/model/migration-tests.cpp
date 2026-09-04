#include <core/model/migration.hpp>

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
        worm::reflection::field("name", &User::name, {.nullable = false})};
    }
  };

  worm::core::Column column(std::string_view name, worm::reflection::FieldMetadata metadata = {})
  {
    metadata.columnName = name;
    return worm::core::Column{metadata, User::table()};
  }
} // namespace

int main()
{
  const std::vector<worm::core::SchemaDifference> differences{
    {
      .kind = worm::core::SchemaDifferenceKind::MissingTable,
      .table = "users",
      .expected = "users",
    },
    {
      .kind = worm::core::SchemaDifferenceKind::MissingColumn,
      .table = "users",
      .column = "name",
    },
    {
      .kind = worm::core::SchemaDifferenceKind::UnexpectedColumn,
      .table = "users",
      .column = "legacy_name",
    },
    {
      .kind = worm::core::SchemaDifferenceKind::PrimaryKeyMismatch,
      .table = "users",
      .expected = "pk_users",
      .actual = "pk_legacy_users",
    },
    {
      .kind = worm::core::SchemaDifferenceKind::UnexpectedTable,
      .table = "audit_log",
    },
    {
      .kind = worm::core::SchemaDifferenceKind::ColumnTypeMismatch,
      .table = "users",
      .column = "age",
      .expected = "int32",
      .actual = "string",
    },
    {
      .kind = worm::core::SchemaDifferenceKind::DefaultExpressionMismatch,
      .table = "users",
      .column = "active",
      .expected = "true",
      .actual = "false",
    },
  };
  const worm::core::MigrationPlan plan = worm::core::generateMigrationPlan(differences);

  if (plan.steps().size() != differences.size() || !plan.requiresManualReview() || plan.executable()) {
    std::cerr << "Migration plan did not preserve review-only semantics.\n";
    return 1;
  }

  if (plan.steps()[0].kind != worm::core::MigrationStepKind::CreateTable ||
      plan.steps()[0].risk != worm::core::MigrationRisk::Ambiguous ||
      plan.steps()[1].kind != worm::core::MigrationStepKind::AddColumn ||
      plan.steps()[2].kind != worm::core::MigrationStepKind::DropColumn ||
      plan.steps()[2].risk != worm::core::MigrationRisk::Destructive ||
      plan.steps()[3].kind != worm::core::MigrationStepKind::ChangePrimaryKey ||
      plan.steps()[3].risk != worm::core::MigrationRisk::Destructive ||
      plan.steps()[4].kind != worm::core::MigrationStepKind::DropTable ||
      plan.steps()[4].risk != worm::core::MigrationRisk::Destructive ||
      plan.steps()[5].kind != worm::core::MigrationStepKind::AlterColumnType ||
      plan.steps()[5].risk != worm::core::MigrationRisk::Destructive ||
      plan.steps()[6].kind != worm::core::MigrationStepKind::AlterColumnDefault ||
      plan.steps()[6].risk != worm::core::MigrationRisk::Ambiguous) {
    std::cerr << "Migration plan did not map schema differences to the expected step kinds.\n";
    return 1;
  }

  if (plan.steps()[1].description.find("name") == std::string::npos ||
      plan.steps()[3].description.find("pk_legacy_users") == std::string::npos ||
      plan.steps()[0].statement.has_value()) {
    std::cerr << "Migration plan did not expose reviewable descriptions without executable SQL.\n";
    return 1;
  }

  const worm::core::TableMetadata matchingTable{User::table(),
    {
      column("id", {.generated = true, .nullable = false}),
      column("name", {.nullable = false}),
    },
    User::primaryKey()};
  const worm::core::SchemaMetadata matchingSchema{worm::core::Schema{}, {matchingTable}};
  const worm::core::MigrationPlan emptyPlan = worm::core::generateMigrationPlan<User>(matchingSchema);

  if (!emptyPlan.empty() || emptyPlan.requiresManualReview() || emptyPlan.executable()) {
    std::cerr << "Migration plan did not remain empty for a matching schema.\n";
    return 1;
  }

  const worm::core::SchemaMetadata missingTableSchema{worm::core::Schema{}, {}};
  const worm::core::MigrationPlan missingTablePlan = worm::core::generateMigrationPlan<User>(missingTableSchema);

  if (missingTablePlan.steps().size() != 1 ||
      missingTablePlan.steps().front().kind != worm::core::MigrationStepKind::CreateTable ||
      !missingTablePlan.steps().front().requiresManualReview()) {
    std::cerr << "Migration plan did not generate a reviewable create-table step for a missing table.\n";
    return 1;
  }

  return 0;
}
