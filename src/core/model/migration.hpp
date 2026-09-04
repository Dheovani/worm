#pragma once

#include <core/model/schema-diff.hpp>
#include <core/query/statement.hpp>

#include <optional>
#include <string>
#include <vector>

namespace worm::core
{
  enum class MigrationStepKind
  {
    CreateTable,
    DropTable,
    AddColumn,
    DropColumn,
    AlterColumnType,
    AlterColumnNullability,
    AlterGeneratedColumn,
    AlterUniqueConstraint,
    AlterColumnDefault,
    AddPrimaryKey,
    ChangePrimaryKey
  };

  enum class MigrationRisk
  {
    Safe,
    Ambiguous,
    Destructive
  };

  struct MigrationStep
  {
    MigrationStepKind kind;
    MigrationRisk risk;
    SchemaDifference difference;
    std::string description;
    std::optional<Statement> statement = std::nullopt;

    [[nodiscard]]
    bool requiresManualReview() const noexcept;

    [[nodiscard]]
    bool executable() const noexcept;
  };

  class MigrationPlan
  {
  public:
    explicit MigrationPlan(std::vector<MigrationStep> steps = {});

    [[nodiscard]]
    const std::vector<MigrationStep>& steps() const noexcept;

    [[nodiscard]]
    bool empty() const noexcept;

    [[nodiscard]]
    bool requiresManualReview() const noexcept;

    [[nodiscard]]
    bool executable() const noexcept;

  private:
    std::vector<MigrationStep> steps_;
  };

  [[nodiscard]]
  MigrationPlan generateMigrationPlan(const std::vector<SchemaDifference>& differences);

  template <PersistableEntity T>
  [[nodiscard]]
  MigrationPlan generateMigrationPlan(const SchemaMetadata& existing)
  {
    return generateMigrationPlan(compareEntityWithSchema<T>(existing));
  }
} // namespace worm::core
