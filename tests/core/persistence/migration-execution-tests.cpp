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

  const worm::core::MigrationArtifact safeArtifact = worm::core::makeMigrationArtifact(
    "20260927115900",
    "create-schema",
    "postgresql",
    {
      {
        .description = "Create application schema",
        .sql = "create schema application",
      },
    });
  const worm::core::MigrationExecutionPlan safe =
    worm::core::compileMigrationExecutionPlan(safeArtifact, worm::core::PgBuilder{});
  if (safe.policy().requiredConfirmation != worm::core::MigrationConfirmation::None) {
    std::cerr << "Safe migration unexpectedly requires confirmation.\n";
    return 1;
  }
  worm::core::authorizeMigrationExecution(safe);

  const worm::core::MigrationExecutionPlan forward =
    worm::core::compileMigrationExecutionPlan(pgArtifact, worm::core::PgBuilder{});
  if (forward.migrationId() != pgArtifact.id() || forward.migrationChecksum() != pgArtifact.checksum() ||
      forward.direction() != worm::core::MigrationDirection::Forward ||
      forward.transactionMode() != worm::core::MigrationTransactionMode::PerMigration || forward.steps().size() != 2 ||
      forward.steps()[0].statement.sql != "create table users (id bigint primary key)" ||
      !forward.steps()[0].statement.parameters.empty() ||
      forward.steps()[1].risk != worm::core::MigrationRisk::Ambiguous ||
      forward.policy().requiredConfirmation != worm::core::MigrationConfirmation::Ambiguous ||
      forward.policy().failureRecovery != worm::core::MigrationFailureRecovery::RollbackTransaction ||
      forward.policy().mayBePartiallyApplied()) {
    std::cerr << "Forward migration execution plan did not preserve its statements or transaction boundary.\n";
    return 1;
  }

  const worm::core::MigrationExecutionPlan rollback = worm::core::compileMigrationExecutionPlan(
    pgArtifact,
    worm::core::PgBuilder{},
    worm::core::MigrationDirection::Rollback);
  if (rollback.direction() != worm::core::MigrationDirection::Rollback || rollback.steps().size() != 1 ||
      rollback.steps().front().statement.sql != "drop table users" ||
      rollback.policy().requiredConfirmation != worm::core::MigrationConfirmation::Destructive) {
    std::cerr << "Rollback migration execution plan was not compiled from the explicit rollback statements.\n";
    return 1;
  }

  const auto mysql = worm::core::compileMigrationExecutionPlan(artifactFor("mysql"), worm::core::MySqlBuilder{});
  const auto sqlite = worm::core::compileMigrationExecutionPlan(artifactFor("sqlite"), worm::core::SqliteBuilder{});
  const auto mssql = worm::core::compileMigrationExecutionPlan(artifactFor("mssql"), worm::core::SqlServerBuilder{});
  if (mysql.transactionMode() != worm::core::MigrationTransactionMode::None ||
      sqlite.transactionMode() != worm::core::MigrationTransactionMode::LockOwned ||
      mssql.transactionMode() != worm::core::MigrationTransactionMode::PerMigration ||
      mysql.policy().failureRecovery != worm::core::MigrationFailureRecovery::ManualReconciliation ||
      !mysql.policy().mayBePartiallyApplied() ||
      sqlite.policy().failureRecovery != worm::core::MigrationFailureRecovery::RollbackLockScope ||
      sqlite.policy().mayBePartiallyApplied() ||
      mssql.policy().failureRecovery != worm::core::MigrationFailureRecovery::RollbackTransaction ||
      mssql.policy().mayBePartiallyApplied()) {
    std::cerr << "Migration transaction modes do not match the selected database dialects.\n";
    return 1;
  }

  bool missingAmbiguousConfirmationRejected = false;
  try {
    worm::core::authorizeMigrationExecution(forward);
  } catch (const worm::MigrationException&) {
    missingAmbiguousConfirmationRejected = true;
  }
  worm::core::authorizeMigrationExecution(forward, worm::core::MigrationConfirmation::Ambiguous);

  bool insufficientDestructiveConfirmationRejected = false;
  try {
    worm::core::authorizeMigrationExecution(rollback, worm::core::MigrationConfirmation::Ambiguous);
  } catch (const worm::MigrationException&) {
    insufficientDestructiveConfirmationRejected = true;
  }
  worm::core::authorizeMigrationExecution(rollback, worm::core::MigrationConfirmation::Destructive);

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

  if (!mismatchRejected || !missingRollbackRejected || !missingAmbiguousConfirmationRejected ||
      !insufficientDestructiveConfirmationRejected) {
    std::cerr << "Migration execution accepted an incompatible plan or insufficient confirmation.\n";
    return 1;
  }

  return 0;
}
