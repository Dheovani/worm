#pragma once

#include <core/model/migration.hpp>
#include <core/model/schema-metadata.hpp>
#include <core/model/schema-snapshot.hpp>
#include <core/query/sql-builder.hpp>
#include <core/query/statement.hpp>

#include <string>
#include <vector>

namespace worm::core
{
  struct MigrationDdlStep
  {
    MigrationStepKind kind;
    MigrationRisk risk;
    SchemaDifference difference;
    std::string description;
    std::vector<Statement> statements;
  };

  class MigrationDdlPlan final
  {
  public:
    explicit MigrationDdlPlan(std::vector<MigrationDdlStep> steps);

    [[nodiscard]]
    const std::vector<MigrationDdlStep>& steps() const noexcept;

    [[nodiscard]]
    std::vector<Statement> statements() const;

    [[nodiscard]]
    bool empty() const noexcept;

  private:
    std::vector<MigrationDdlStep> steps_;
  };

  [[nodiscard]]
  MigrationDdlPlan compileMigrationDdl(
    const MigrationPlan& plan,
    const SchemaMetadata& expected,
    const SchemaSnapshot& actual,
    const SqlBuilder& sqlBuilder);
} // namespace worm::core
