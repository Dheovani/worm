#pragma once

#include <core/model/migration-artifact.hpp>
#include <core/query/sql-builder.hpp>
#include <core/query/statement.hpp>

#include <string>
#include <vector>

namespace worm::core
{
  enum class MigrationDirection
  {
    Forward,
    Rollback
  };

  enum class MigrationConfirmation
  {
    None,
    Ambiguous,
    Destructive
  };

  enum class MigrationFailureRecovery
  {
    RollbackTransaction,
    RollbackLockScope,
    ManualReconciliation
  };

  struct MigrationExecutionPolicy
  {
    MigrationConfirmation requiredConfirmation;
    MigrationFailureRecovery failureRecovery;

    [[nodiscard]]
    bool mayBePartiallyApplied() const noexcept;

    friend bool operator==(const MigrationExecutionPolicy&, const MigrationExecutionPolicy&) = default;
  };

  struct MigrationExecutionStep
  {
    std::string description;
    Statement statement;
    MigrationRisk risk;

    friend bool operator==(const MigrationExecutionStep&, const MigrationExecutionStep&) = default;
  };

  class MigrationExecutionPlan final
  {
  public:
    [[nodiscard]]
    const std::string& migrationId() const noexcept;

    [[nodiscard]]
    const std::string& migrationChecksum() const noexcept;

    [[nodiscard]]
    MigrationDirection direction() const noexcept;

    [[nodiscard]]
    MigrationTransactionMode transactionMode() const noexcept;

    [[nodiscard]]
    const std::vector<MigrationExecutionStep>& steps() const noexcept;

    [[nodiscard]]
    MigrationExecutionPolicy policy() const noexcept;

  private:
    friend MigrationExecutionPlan compileMigrationExecutionPlan(
      const MigrationArtifact& artifact,
      const SqlBuilder& sqlBuilder,
      MigrationDirection direction);

    MigrationExecutionPlan(
      std::string migrationId,
      std::string migrationChecksum,
      MigrationDirection direction,
      MigrationTransactionMode transactionMode,
      std::vector<MigrationExecutionStep> steps);

    std::string migrationId_;
    std::string migrationChecksum_;
    MigrationDirection direction_;
    MigrationTransactionMode transactionMode_;
    std::vector<MigrationExecutionStep> steps_;
  };

  [[nodiscard]]
  MigrationExecutionPlan compileMigrationExecutionPlan(
    const MigrationArtifact& artifact,
    const SqlBuilder& sqlBuilder,
    MigrationDirection direction = MigrationDirection::Forward);

  void authorizeMigrationExecution(
    const MigrationExecutionPlan& plan,
    MigrationConfirmation confirmation = MigrationConfirmation::None);
} // namespace worm::core
