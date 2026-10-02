#include <core/persistence/migration-execution.hpp>

#include <errors/migration-exception.hpp>
#include <utils/logger.hpp>

#include <string_view>
#include <utility>

namespace worm::core
{
  namespace
  {
    [[nodiscard]]
    MigrationConfirmation confirmationFor(MigrationRisk risk) noexcept
    {
      switch (risk) {
      case MigrationRisk::Safe:
        return MigrationConfirmation::None;
      case MigrationRisk::Ambiguous:
        return MigrationConfirmation::Ambiguous;
      case MigrationRisk::Destructive:
        return MigrationConfirmation::Destructive;
      }

      return MigrationConfirmation::Destructive;
    }

    [[nodiscard]]
    int confirmationRank(MigrationConfirmation confirmation) noexcept
    {
      switch (confirmation) {
      case MigrationConfirmation::None:
        return 0;
      case MigrationConfirmation::Ambiguous:
        return 1;
      case MigrationConfirmation::Destructive:
        return 2;
      }

      return -1;
    }

    [[nodiscard]]
    std::string_view confirmationName(MigrationConfirmation confirmation) noexcept
    {
      switch (confirmation) {
      case MigrationConfirmation::None:
        return "none";
      case MigrationConfirmation::Ambiguous:
        return "ambiguous";
      case MigrationConfirmation::Destructive:
        return "destructive";
      }

      return "unknown";
    }

    [[nodiscard]]
    MigrationFailureRecovery failureRecoveryFor(MigrationTransactionMode mode) noexcept
    {
      switch (mode) {
      case MigrationTransactionMode::PerMigration:
        return MigrationFailureRecovery::RollbackTransaction;
      case MigrationTransactionMode::LockOwned:
        return MigrationFailureRecovery::RollbackLockScope;
      case MigrationTransactionMode::None:
        return MigrationFailureRecovery::ManualReconciliation;
      }

      return MigrationFailureRecovery::ManualReconciliation;
    }
  } // namespace

  bool MigrationExecutionPolicy::mayBePartiallyApplied() const noexcept
  {
    return failureRecovery == MigrationFailureRecovery::ManualReconciliation;
  }

  MigrationExecutionPlan::MigrationExecutionPlan(
    std::string migrationId,
    std::string migrationChecksum,
    MigrationDirection direction,
    MigrationTransactionMode transactionMode,
    std::vector<MigrationExecutionStep> steps)
    : migrationId_(std::move(migrationId)),
      migrationChecksum_(std::move(migrationChecksum)),
      direction_(direction),
      transactionMode_(transactionMode),
      steps_(std::move(steps))
  {}

  const std::string& MigrationExecutionPlan::migrationId() const noexcept
  {
    return migrationId_;
  }

  const std::string& MigrationExecutionPlan::migrationChecksum() const noexcept
  {
    return migrationChecksum_;
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

  MigrationExecutionPolicy MigrationExecutionPlan::policy() const noexcept
  {
    MigrationConfirmation requiredConfirmation = MigrationConfirmation::None;
    for (const MigrationExecutionStep& step : steps_) {
      const MigrationConfirmation stepConfirmation = confirmationFor(step.risk);
      if (confirmationRank(stepConfirmation) > confirmationRank(requiredConfirmation)) {
        requiredConfirmation = stepConfirmation;
      }
    }

    return {
      .requiredConfirmation = requiredConfirmation,
      .failureRecovery = failureRecoveryFor(transactionMode_),
    };
  }

  MigrationExecutionPlan compileMigrationExecutionPlan(
    const MigrationArtifact& artifact,
    const SqlBuilder& sqlBuilder,
    MigrationDirection direction)
  {
    validateMigrationArtifact(artifact);
    logger.log(
      LogLevel::Debug,
      "Migration execution plan compilation started.",
      {
        {"migration", artifact.id()},
        {"direction", direction == MigrationDirection::Forward ? "forward" : "rollback"},
      });

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
      logger.log(
        LogLevel::Trace,
        "Migration execution step planned.",
        {
          {"migration", artifact.id()},
          {"description", statement.description},
          {"risk", std::to_string(static_cast<int>(statement.risk))},
        });
      steps.push_back(
        {
          .description = statement.description,
          .statement = Statement{statement.sql},
          .risk = statement.risk,
        });
    }

    logger.log(
      LogLevel::Debug,
      "Migration execution plan compiled.",
      {
        {"migration", artifact.id()},
        {"steps", std::to_string(steps.size())},
      });
    return {
      artifact.id(),
      artifact.checksum(),
      direction,
      sqlBuilder.migrationTransactionMode(),
      std::move(steps),
    };
  }

  void authorizeMigrationExecution(const MigrationExecutionPlan& plan, MigrationConfirmation confirmation)
  {
    const MigrationConfirmation required = plan.policy().requiredConfirmation;
    if (confirmationRank(confirmation) < confirmationRank(required)) {
      throw MigrationException(
        "Migration '{}' requires '{}' confirmation, but execution was authorized only through '{}'.",
        plan.migrationId(),
        confirmationName(required),
        confirmationName(confirmation));
    }
  }
} // namespace worm::core
