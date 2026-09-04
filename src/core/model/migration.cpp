#include <core/model/migration.hpp>

#include <string_view>
#include <utility>

namespace worm::core
{
  bool MigrationStep::requiresManualReview() const noexcept
  {
    return risk != MigrationRisk::Safe || !statement.has_value();
  }

  bool MigrationStep::executable() const noexcept
  {
    return statement.has_value() && !requiresManualReview();
  }

  MigrationPlan::MigrationPlan(std::vector<MigrationStep> steps)
    : steps_(std::move(steps))
  {}

  const std::vector<MigrationStep>& MigrationPlan::steps() const noexcept
  {
    return steps_;
  }

  bool MigrationPlan::empty() const noexcept
  {
    return steps_.empty();
  }

  bool MigrationPlan::requiresManualReview() const noexcept
  {
    for (const MigrationStep& step : steps_) {
      if (step.requiresManualReview()) {
        return true;
      }
    }

    return false;
  }

  bool MigrationPlan::executable() const noexcept
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

  namespace
  {
    [[nodiscard]]
    std::string tableName(const SchemaDifference& difference)
    {
      if (difference.schema.empty()) {
        return difference.table;
      }

      return difference.schema + "." + difference.table;
    }

    [[nodiscard]]
    std::string migrationDescription(const SchemaDifference& difference, std::string_view action)
    {
      std::string description{action};
      description += " on table '";
      description += tableName(difference);
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
    MigrationStep migrationStepFor(const SchemaDifference& difference)
    {
      switch (difference.kind) {
      case SchemaDifferenceKind::MissingTable:
        return {
          .kind = MigrationStepKind::CreateTable,
          .risk = MigrationRisk::Ambiguous,
          .difference = difference,
          .description = migrationDescription(difference, "Create missing table"),
        };

      case SchemaDifferenceKind::UnexpectedTable:
        return {
          .kind = MigrationStepKind::DropTable,
          .risk = MigrationRisk::Destructive,
          .difference = difference,
          .description = migrationDescription(difference, "Drop unexpected table"),
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

      case SchemaDifferenceKind::ColumnTypeMismatch:
        return {
          .kind = MigrationStepKind::AlterColumnType,
          .risk = MigrationRisk::Destructive,
          .difference = difference,
          .description = migrationDescription(difference, "Alter column type"),
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

      case SchemaDifferenceKind::DefaultExpressionMismatch:
        return {
          .kind = MigrationStepKind::AlterColumnDefault,
          .risk = MigrationRisk::Ambiguous,
          .difference = difference,
          .description = migrationDescription(difference, "Alter column default"),
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
  } // namespace

  MigrationPlan generateMigrationPlan(const std::vector<SchemaDifference>& differences)
  {
    std::vector<MigrationStep> steps;
    steps.reserve(differences.size());

    for (const SchemaDifference& difference : differences) {
      steps.push_back(migrationStepFor(difference));
    }

    return MigrationPlan{std::move(steps)};
  }
} // namespace worm::core
