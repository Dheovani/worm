#include <core/model/migration.hpp>
#include <core/model/schema-diff.hpp>
#include <core/model/schema-metadata.hpp>
#include <core/model/schema-snapshot.hpp>
#include <core/query/migration-ddl.hpp>
#include <core/query/sql-builder.hpp>

#include <errors/migration-exception.hpp>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
  const worm::core::Table users{worm::core::Schema{"public"}, "users"};
  const worm::core::Column id{worm::reflection::FieldMetadata{.columnName = "id", .generated = true, .nullable = false},
    users};
  const worm::core::Column name{worm::reflection::FieldMetadata{
                                  .columnName = "name",
                                  .defaultExpression = "'unknown'",
                                  .nullable = false,
                                },
    users};
  const worm::core::Column active{worm::reflection::FieldMetadata{.columnName = "active", .nullable = false}, users};
  const worm::core::Column email{"email", users};

  class EmptyDdlBuilder final : public worm::core::SqlBuilder
  {
  public:
    std::vector<worm::core::Statement> compileMigrationStep(
      const worm::core::MigrationStep&,
      const worm::core::SchemaMetadata&,
      const worm::core::SchemaSnapshot&) const override
    {
      return {{}};
    }
  };

  class ParameterizedDdlBuilder final : public worm::core::SqlBuilder
  {
  public:
    std::vector<worm::core::Statement> compileMigrationStep(
      const worm::core::MigrationStep&,
      const worm::core::SchemaMetadata&,
      const worm::core::SchemaSnapshot&) const override
    {
      return {{"create table unsafe (id bigint)", {worm::core::Parameter{std::int64_t{1}}}}};
    }
  };

  worm::core::TableMetadata usersMetadata()
  {
    return worm::core::TableMetadata{
      users,
      {
        worm::core::ColumnMetadata{id, {.kind = worm::core::ColumnTypeKind::Int64}},
        worm::core::ColumnMetadata{name, {.kind = worm::core::ColumnTypeKind::String, .length = std::size_t{120}}},
        worm::core::ColumnMetadata{active, {.kind = worm::core::ColumnTypeKind::Boolean}},
        worm::core::ColumnMetadata{email, {.kind = worm::core::ColumnTypeKind::String}},
      },
      worm::core::PrimaryKey{"pk_users", {id}},
      {worm::core::Index{"idx_users_email", {{email}}}},
    };
  }

  worm::core::SchemaTableSnapshot actualUsers()
  {
    return {
      .schema = "public",
      .name = "users",
      .columns =
        {
          {
            .name = "id",
            .type = {.kind = worm::core::ColumnTypeKind::Int32},
            .nullable = true,
            .generated = true,
          },
          {
            .name = "name",
            .type = {.kind = worm::core::ColumnTypeKind::String, .length = std::size_t{60}},
            .defaultExpression = "'legacy'",
            .nullable = true,
          },
          {
            .name = "active",
            .type = {.kind = worm::core::ColumnTypeKind::Boolean},
            .defaultExpression = "true",
            .nullable = false,
          },
          {
            .name = "legacy",
            .type = {.kind = worm::core::ColumnTypeKind::String},
          },
        },
    };
  }

  bool containsSql(const std::vector<worm::core::Statement>& statements, std::string_view fragment)
  {
    return std::ranges::any_of(statements, [fragment](const worm::core::Statement& statement) {
      return statement.sql.find(fragment) != std::string::npos;
    });
  }

  template <typename Function>
  bool rejectsUnsupported(Function&& function)
  {
    try {
      function();
    } catch (const worm::MigrationException&) {
      return true;
    }
    return false;
  }
} // namespace

int main()
{
  const worm::core::SchemaMetadata expected{worm::core::Schema{"public"}, {usersMetadata()}};
  const worm::core::SchemaSnapshot actual{{actualUsers()}};
  const std::vector<worm::core::SchemaDifference> differences = worm::core::compareSchemas(expected, actual);
  const worm::core::MigrationPlan plan = worm::core::generateMigrationPlan(differences);
  const worm::core::MigrationDdlPlan pgDdl =
    worm::core::compileMigrationDdl(plan, expected, actual, worm::core::PgBuilder{});
  const std::vector<worm::core::Statement> pgStatements = pgDdl.statements();
  const bool parameterFree = std::ranges::all_of(pgStatements, [](const worm::core::Statement& statement) {
    return statement.parameters.empty();
  });

  if (!parameterFree || pgDdl.steps().size() != differences.size() ||
      !containsSql(pgStatements, "alter column \"id\" type bigint") ||
      !containsSql(pgStatements, "alter column \"id\" set not null") ||
      !containsSql(pgStatements, "alter column \"name\" set default 'unknown'") ||
      !containsSql(pgStatements, "alter column \"active\" drop default") ||
      !containsSql(pgStatements, "add column \"email\" text") || !containsSql(pgStatements, "drop column \"legacy\"") ||
      !containsSql(pgStatements, "constraint \"pk_users\" primary key (\"id\")")) {
    std::cerr << "PostgreSQL migration DDL did not cover the supported schema differences.\n";
    return 1;
  }

  const worm::core::SchemaSnapshot emptySchema{};
  const worm::core::MigrationDdlPlan createDdl = worm::core::compileMigrationDdl(
    worm::core::generateMigrationPlan(worm::core::compareSchemas(expected, emptySchema)),
    expected,
    emptySchema,
    worm::core::PgBuilder{});
  if (createDdl.statements().size() != 2 || !containsSql(createDdl.statements(), "create table \"public\".\"users\"") ||
      !containsSql(createDdl.statements(), "create index \"idx_users_email\"")) {
    std::cerr << "CREATE TABLE migration did not preserve all dialect-generated statements.\n";
    return 1;
  }

  const worm::core::SchemaMetadata noTables{worm::core::Schema{"public"}, {}};
  const worm::core::MigrationDdlPlan dropDdl = worm::core::compileMigrationDdl(
    worm::core::generateMigrationPlan(worm::core::compareSchemas(noTables, actual)),
    noTables,
    actual,
    worm::core::PgBuilder{});
  if (dropDdl.statements().size() != 1 || dropDdl.statements().front().sql != "drop table \"public\".\"users\"") {
    std::cerr << "DROP TABLE migration was not rendered with qualified identifiers.\n";
    return 1;
  }

  const worm::core::MigrationDdlPlan mysqlDdl =
    worm::core::compileMigrationDdl(plan, expected, actual, worm::core::MySqlBuilder{});
  if (!containsSql(mysqlDdl.statements(), "alter table `public`.`users` modify column `id` bigint auto_increment") ||
      !containsSql(mysqlDdl.statements(), "add primary key (`id`)")) {
    std::cerr << "MySQL migration DDL did not use full column definitions or primary-key syntax.\n";
    return 1;
  }

  worm::core::SchemaTableSnapshot mysqlChangedPrimaryKey = actualUsers();
  mysqlChangedPrimaryKey.primaryKey = {"legacy"};
  const worm::core::MigrationPlan changePrimaryKeyPlan = worm::core::generateMigrationPlan(
    {
      {
        .kind = worm::core::SchemaDifferenceKind::PrimaryKeyMismatch,
        .schema = "public",
        .table = "users",
      },
    });
  const worm::core::MigrationDdlPlan mysqlPrimaryKeyDdl = worm::core::compileMigrationDdl(
    changePrimaryKeyPlan,
    expected,
    worm::core::SchemaSnapshot{{std::move(mysqlChangedPrimaryKey)}},
    worm::core::MySqlBuilder{});
  if (!containsSql(mysqlPrimaryKeyDdl.statements(), "drop primary key,add primary key (`id`)")) {
    std::cerr << "MySQL primary-key replacement was not rendered with native syntax.\n";
    return 1;
  }

  const worm::core::MigrationPlan sqlServerPlan = worm::core::generateMigrationPlan(
    {
      {
        .kind = worm::core::SchemaDifferenceKind::ColumnTypeMismatch,
        .schema = "public",
        .table = "users",
        .column = "id",
      },
      {
        .kind = worm::core::SchemaDifferenceKind::NullableMismatch,
        .schema = "public",
        .table = "users",
        .column = "id",
      },
    });
  const worm::core::MigrationDdlPlan sqlServerDdl =
    worm::core::compileMigrationDdl(sqlServerPlan, expected, actual, worm::core::SqlServerBuilder{});
  if (!containsSql(sqlServerDdl.statements(), "alter column [id] bigint not null")) {
    std::cerr << "SQL Server migration DDL did not restate type and nullability.\n";
    return 1;
  }

  const worm::core::SchemaDifference uniqueDifference{
    .kind = worm::core::SchemaDifferenceKind::UniqueMismatch,
    .schema = "public",
    .table = "users",
    .column = "name",
  };
  const worm::core::MigrationPlan uniquePlan = worm::core::generateMigrationPlan({uniqueDifference});
  const bool pgUniqueRejected = rejectsUnsupported(
    [&] { static_cast<void>(worm::core::compileMigrationDdl(uniquePlan, expected, actual, worm::core::PgBuilder{})); });

  const worm::core::MigrationPlan generatedPlan = worm::core::generateMigrationPlan(
    {
      {
        .kind = worm::core::SchemaDifferenceKind::GeneratedMismatch,
        .schema = "public",
        .table = "users",
        .column = "id",
      },
    });
  const bool generatedRejected = rejectsUnsupported([&] {
    static_cast<void>(worm::core::compileMigrationDdl(generatedPlan, expected, actual, worm::core::PgBuilder{}));
  });

  const worm::core::MigrationPlan sqliteRebuildPlan = worm::core::generateMigrationPlan(
    {
      {
        .kind = worm::core::SchemaDifferenceKind::ColumnTypeMismatch,
        .schema = "public",
        .table = "users",
        .column = "id",
      },
    });
  const bool sqliteRebuildRejected = rejectsUnsupported([&] {
    static_cast<void>(
      worm::core::compileMigrationDdl(sqliteRebuildPlan, expected, actual, worm::core::SqliteBuilder{}));
  });

  const worm::core::MigrationPlan sqliteAddColumnPlan = worm::core::generateMigrationPlan(
    {
      {
        .kind = worm::core::SchemaDifferenceKind::MissingColumn,
        .schema = "public",
        .table = "users",
        .column = "email",
      },
    });
  const worm::core::MigrationDdlPlan sqliteAddColumnDdl =
    worm::core::compileMigrationDdl(sqliteAddColumnPlan, expected, actual, worm::core::SqliteBuilder{});
  if (!containsSql(sqliteAddColumnDdl.statements(), "add column \"email\" text")) {
    std::cerr << "SQLite did not compile its supported ADD COLUMN operation.\n";
    return 1;
  }

  const worm::core::MigrationPlan sqlServerDefaultPlan = worm::core::generateMigrationPlan(
    {
      {
        .kind = worm::core::SchemaDifferenceKind::DefaultExpressionMismatch,
        .schema = "public",
        .table = "users",
        .column = "name",
      },
    });
  const bool sqlServerDefaultRejected = rejectsUnsupported([&] {
    static_cast<void>(
      worm::core::compileMigrationDdl(sqlServerDefaultPlan, expected, actual, worm::core::SqlServerBuilder{}));
  });
  const bool emptyStatementRejected = rejectsUnsupported(
    [&] { static_cast<void>(worm::core::compileMigrationDdl(plan, expected, actual, EmptyDdlBuilder{})); });
  const bool parameterizedStatementRejected = rejectsUnsupported(
    [&] { static_cast<void>(worm::core::compileMigrationDdl(plan, expected, actual, ParameterizedDdlBuilder{})); });

  if (!pgUniqueRejected || !generatedRejected || !sqliteRebuildRejected || !sqlServerDefaultRejected ||
      !emptyStatementRejected || !parameterizedStatementRejected) {
    std::cerr << "Migration DDL compiler guessed a transformation without the required metadata.\n";
    return 1;
  }

  return 0;
}
