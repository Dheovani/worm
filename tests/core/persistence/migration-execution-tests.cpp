#include <core/model/migration-artifact.hpp>
#include <core/persistence/migration-execution.hpp>
#include <core/query/sql-builder.hpp>

#include <errors/migration-exception.hpp>

#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{
  worm::core::MigrationArtifact artifactFor(
    std::string database,
    std::optional<std::vector<worm::core::MigrationStatement>> rollback = std::nullopt)
  {
    return worm::core::makeMigrationArtifact(
      "20260927120000",
      "create-users",
      std::move(database),
      {
        {
          .description = "Create users table",
          .sql = "create table users (id bigint primary key)",
          .risk = worm::core::MigrationRisk::Safe,
        },
        {
          .description = "Create users name index",
          .sql = "create index users_name_idx on users (name)",
          .risk = worm::core::MigrationRisk::Ambiguous,
        },
      },
      std::move(rollback));
  }
} // namespace

int main()
{
  const worm::core::MigrationArtifact pgArtifact = artifactFor(
    "postgresql",
    std::vector<worm::core::MigrationStatement>{
      {
        .description = "Drop users table",
        .sql = "drop table users",
        .risk = worm::core::MigrationRisk::Destructive,
      },
    });

  const worm::core::MigrationExecutionPlan forward =
    worm::core::compileMigrationExecutionPlan(pgArtifact, worm::core::PgBuilder{});
  if (forward.migrationId() != pgArtifact.id() || forward.direction() != worm::core::MigrationDirection::Forward ||
      forward.transactionMode() != worm::core::MigrationTransactionMode::PerMigration || forward.steps().size() != 2 ||
      forward.steps()[0].statement.sql != "create table users (id bigint primary key)" ||
      !forward.steps()[0].statement.parameters.empty() ||
      forward.steps()[1].risk != worm::core::MigrationRisk::Ambiguous) {
    std::cerr << "Forward migration execution plan did not preserve its statements or transaction boundary.\n";
    return 1;
  }

  const worm::core::MigrationExecutionPlan rollback = worm::core::compileMigrationExecutionPlan(
    pgArtifact,
    worm::core::PgBuilder{},
    worm::core::MigrationDirection::Rollback);
  if (rollback.direction() != worm::core::MigrationDirection::Rollback || rollback.steps().size() != 1 ||
      rollback.steps().front().statement.sql != "drop table users") {
    std::cerr << "Rollback migration execution plan was not compiled from the explicit rollback statements.\n";
    return 1;
  }

  const auto mysql = worm::core::compileMigrationExecutionPlan(artifactFor("mysql"), worm::core::MySqlBuilder{});
  const auto sqlite = worm::core::compileMigrationExecutionPlan(artifactFor("sqlite"), worm::core::SqliteBuilder{});
  const auto mssql = worm::core::compileMigrationExecutionPlan(artifactFor("mssql"), worm::core::SqlServerBuilder{});
  if (mysql.transactionMode() != worm::core::MigrationTransactionMode::None ||
      sqlite.transactionMode() != worm::core::MigrationTransactionMode::LockOwned ||
      mssql.transactionMode() != worm::core::MigrationTransactionMode::PerMigration) {
    std::cerr << "Migration transaction modes do not match the selected database dialects.\n";
    return 1;
  }

  bool mismatchRejected = false;
  try {
    static_cast<void>(worm::core::compileMigrationExecutionPlan(pgArtifact, worm::core::MySqlBuilder{}));
  } catch (const worm::MigrationException&) {
    mismatchRejected = true;
  }

  bool missingRollbackRejected = false;
  try {
    static_cast<void>(worm::core::compileMigrationExecutionPlan(
      artifactFor("postgresql"),
      worm::core::PgBuilder{},
      worm::core::MigrationDirection::Rollback));
  } catch (const worm::MigrationException&) {
    missingRollbackRejected = true;
  }

  if (!mismatchRejected || !missingRollbackRejected) {
    std::cerr << "Migration compilation accepted an incompatible database or an unavailable rollback.\n";
    return 1;
  }

  return 0;
}
