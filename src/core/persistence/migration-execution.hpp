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
    MigrationDirection direction() const noexcept;

    [[nodiscard]]
    MigrationTransactionMode transactionMode() const noexcept;

    [[nodiscard]]
    const std::vector<MigrationExecutionStep>& steps() const noexcept;

  private:
    friend MigrationExecutionPlan compileMigrationExecutionPlan(
      const MigrationArtifact& artifact,
      const SqlBuilder& sqlBuilder,
      MigrationDirection direction);

    MigrationExecutionPlan(
      std::string migrationId,
      MigrationDirection direction,
      MigrationTransactionMode transactionMode,
      std::vector<MigrationExecutionStep> steps);

    std::string migrationId_;
    MigrationDirection direction_;
    MigrationTransactionMode transactionMode_;
    std::vector<MigrationExecutionStep> steps_;
  };

  [[nodiscard]]
  MigrationExecutionPlan compileMigrationExecutionPlan(
    const MigrationArtifact& artifact,
    const SqlBuilder& sqlBuilder,
    MigrationDirection direction = MigrationDirection::Forward);
} // namespace worm::core
