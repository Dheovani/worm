#include <core/query/migration-ddl.hpp>

#include <errors/migration-exception.hpp>

#include <utility>

namespace worm::core
{
  MigrationDdlPlan::MigrationDdlPlan(std::vector<MigrationDdlStep> steps)
    : steps_(std::move(steps))
  {}

  const std::vector<MigrationDdlStep>& MigrationDdlPlan::steps() const noexcept
  {
    return steps_;
  }

  std::vector<Statement> MigrationDdlPlan::statements() const
  {
    std::vector<Statement> statements;
    for (const MigrationDdlStep& step : steps_) {
      statements.insert(statements.end(), step.statements.begin(), step.statements.end());
    }
    return statements;
  }

  bool MigrationDdlPlan::empty() const noexcept
  {
    return steps_.empty();
  }

  MigrationDdlPlan compileMigrationDdl(
    const MigrationPlan& plan,
    const SchemaMetadata& expected,
    const SchemaSnapshot& actual,
    const SqlBuilder& sqlBuilder)
  {
    std::vector<MigrationDdlStep> compiled;
    compiled.reserve(plan.steps().size());

    for (const MigrationStep& step : plan.steps()) {
      std::vector<Statement> statements = sqlBuilder.compileMigrationStep(step, expected, actual);
      if (statements.empty()) {
        throw MigrationException("Migration step '{}' did not produce any DDL statements.", step.description);
      }

      for (const Statement& statement : statements) {
        if (statement.sql.empty()) {
          throw MigrationException("Migration step '{}' produced an empty DDL statement.", step.description);
        }

        if (!statement.parameters.empty()) {
          throw MigrationException(
            "Migration step '{}' produced parameterized DDL, which cannot be reviewed as complete SQL.",
            step.description);
        }
      }

      compiled.push_back(
        {
          .kind = step.kind,
          .risk = step.risk,
          .difference = step.difference,
          .description = step.description,
          .statements = std::move(statements),
        });
    }

    return MigrationDdlPlan{std::move(compiled)};
  }
} // namespace worm::core
