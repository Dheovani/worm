#include <core/persistence/migration-execution.hpp>

#include <errors/migration-exception.hpp>

#include <utility>

namespace worm::core
{
  MigrationExecutionPlan::MigrationExecutionPlan(
    std::string migrationId,
    MigrationDirection direction,
    MigrationTransactionMode transactionMode,
    std::vector<MigrationExecutionStep> steps)
    : migrationId_(std::move(migrationId)),
      direction_(direction),
      transactionMode_(transactionMode),
      steps_(std::move(steps))
  {}

  const std::string& MigrationExecutionPlan::migrationId() const noexcept
  {
    return migrationId_;
  }

  MigrationDirection MigrationExecutionPlan::direction() const noexcept
  {
    return direction_;
  }

  MigrationTransactionMode MigrationExecutionPlan::transactionMode() const noexcept
  {
    return transactionMode_;
  }

  const std::vector<MigrationExecutionStep>& MigrationExecutionPlan::steps() const noexcept
  {
    return steps_;
  }

  MigrationExecutionPlan compileMigrationExecutionPlan(
    const MigrationArtifact& artifact,
    const SqlBuilder& sqlBuilder,
    MigrationDirection direction)
  {
    validateMigrationArtifact(artifact);

    if (artifact.database() != sqlBuilder.databaseName()) {
      throw MigrationException(
        "Migration '{}' targets database '{}', but the selected SQL builder targets '{}'.",
        artifact.id(),
        artifact.database(),
        sqlBuilder.databaseName());
    }

    const std::vector<MigrationStatement>* source = &artifact.forward();
    if (direction == MigrationDirection::Rollback) {
      if (!artifact.rollback().has_value()) {
        throw MigrationException("Migration '{}' does not define rollback statements.", artifact.id());
      }
      source = &*artifact.rollback();
    }

    std::vector<MigrationExecutionStep> steps;
    steps.reserve(source->size());
    for (const MigrationStatement& statement : *source) {
      steps.push_back(
        {
          .description = statement.description,
          .statement = Statement{statement.sql},
          .risk = statement.risk,
        });
    }

    return {artifact.id(), direction, sqlBuilder.migrationTransactionMode(), std::move(steps)};
  }
} // namespace worm::core
