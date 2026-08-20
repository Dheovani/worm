#pragma once

#include <core/model/schema-diff.hpp>
#include <core/query/statement.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace worm::core
{
  enum class MigrationStepKind
  {
    CreateTable,
    AddColumn,
    DropColumn,
    AlterColumnNullability,
    AlterGeneratedColumn,
    AlterUniqueConstraint,
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
    bool requiresManualReview() const noexcept
    {
      return risk != MigrationRisk::Safe || !statement.has_value();
    }

    [[nodiscard]]
    bool executable() const noexcept
    {
      return statement.has_value() && !requiresManualReview();
    }
  };

  class MigrationPlan
  {
  public:
    explicit MigrationPlan(std::vector<MigrationStep> steps = {})
      : steps_(std::move(steps))
    {
    }

    [[nodiscard]]
    const std::vector<MigrationStep>& steps() const noexcept
    {
      return steps_;
    }

    [[nodiscard]]
    bool empty() const noexcept
    {
      return steps_.empty();
    }

    [[nodiscard]]
    bool requiresManualReview() const noexcept
    {
      for (const MigrationStep& step : steps_) {
        if (step.requiresManualReview()) {
          return true;
        }
      }

      return false;
    }

    [[nodiscard]]
    bool executable() const noexcept
    {
      if (steps_.empty()) {
        return false;
      }

      for (const MigrationStep& step : steps_) {
        if (!step.executable()) {
          return false;
        }
      }

      return true;
    }

  private:
    std::vector<MigrationStep> steps_;
  };

  namespace detail
  {
    [[nodiscard]]
    inline std::string tableName(Table table)
    {
      if (table.schema().empty()) {
        return std::string{table.name()};
      }

      return std::string{table.schema().name()} + "." + std::string{table.name()};
    }

    [[nodiscard]]
    inline std::string migrationDescription(const SchemaDifference& difference, std::string_view action)
    {
      std::string description{action};
      description += " on table '";
      description += tableName(difference.table);
      description += "'";

      if (!difference.column.empty()) {
        description += " for column '";
        description += difference.column;
        description += "'";
      }

      if (!difference.expected.empty() || !difference.actual.empty()) {
        description += " (expected: '";
        description += difference.expected;
        description += "', actual: '";
        description += difference.actual;
        description += "')";
      }

      return description;
    }

    [[nodiscard]]
    inline MigrationStep migrationStepFor(const SchemaDifference& difference)
    {
      switch (difference.kind) {
      case SchemaDifferenceKind::MissingTable:
        return {
          .kind = MigrationStepKind::CreateTable,
          .risk = MigrationRisk::Ambiguous,
          .difference = difference,
          .description = migrationDescription(difference, "Create missing table"),
        };

      case SchemaDifferenceKind::MissingColumn:
        return {
          .kind = MigrationStepKind::AddColumn,
          .risk = MigrationRisk::Ambiguous,
          .difference = difference,
          .description = migrationDescription(difference, "Add missing column"),
        };

      case SchemaDifferenceKind::UnexpectedColumn:
        return {
          .kind = MigrationStepKind::DropColumn,
          .risk = MigrationRisk::Destructive,
          .difference = difference,
          .description = migrationDescription(difference, "Drop unexpected column"),
        };

      case SchemaDifferenceKind::NullableMismatch:
        return {
          .kind = MigrationStepKind::AlterColumnNullability,
          .risk = MigrationRisk::Ambiguous,
          .difference = difference,
          .description = migrationDescription(difference, "Alter column nullability"),
        };

      case SchemaDifferenceKind::GeneratedMismatch:
        return {
          .kind = MigrationStepKind::AlterGeneratedColumn,
          .risk = MigrationRisk::Ambiguous,
          .difference = difference,
          .description = migrationDescription(difference, "Alter generated column metadata"),
        };

      case SchemaDifferenceKind::UniqueMismatch:
        return {
          .kind = MigrationStepKind::AlterUniqueConstraint,
          .risk = MigrationRisk::Ambiguous,
          .difference = difference,
          .description = migrationDescription(difference, "Alter unique constraint"),
        };

      case SchemaDifferenceKind::MissingPrimaryKey:
        return {
          .kind = MigrationStepKind::AddPrimaryKey,
          .risk = MigrationRisk::Ambiguous,
          .difference = difference,
          .description = migrationDescription(difference, "Add missing primary key"),
        };

      case SchemaDifferenceKind::PrimaryKeyMismatch:
        return {
          .kind = MigrationStepKind::ChangePrimaryKey,
          .risk = MigrationRisk::Destructive,
          .difference = difference,
          .description = migrationDescription(difference, "Change primary key"),
        };
      }

      return {
        .kind = MigrationStepKind::CreateTable,
        .risk = MigrationRisk::Ambiguous,
        .difference = difference,
        .description = migrationDescription(difference, "Review unsupported schema difference"),
      };
    }
  } // namespace detail

  [[nodiscard]]
  inline MigrationPlan generateMigrationPlan(const std::vector<SchemaDifference>& differences)
  {
    std::vector<MigrationStep> steps;
    steps.reserve(differences.size());

    for (const SchemaDifference& difference : differences) {
      steps.push_back(detail::migrationStepFor(difference));
    }

    return MigrationPlan{std::move(steps)};
  }

  template <PersistableEntity T>
  [[nodiscard]]
  MigrationPlan generateMigrationPlan(const SchemaMetadata& existing)
  {
    return generateMigrationPlan(compareEntityWithSchema<T>(existing));
  }
} // namespace worm::core
