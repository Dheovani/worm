#include <generator/check.hpp>

#include <errors/invalid-argument-exception.hpp>

#include <iostream>
#include <memory>

namespace
{
  worm::cli::generator::SchemaManifest manifest()
  {
    return {
      .entities = {{
        .name = "User",
        .table =
          {
            .schema = "public",
            .name = "users",
            .columns =
              {
                {
                  .name = "id",
                  .type = {.kind = worm::core::ColumnTypeKind::Int64},
                  .nullable = false,
                  .generated = true,
                },
                {
                  .name = "email",
                  .type = {.kind = worm::core::ColumnTypeKind::String},
                  .nullable = false,
                  .unique = true,
                },
              },
            .primaryKey = {"id"},
          },
      }},
    };
  }

  worm::core::SchemaSnapshot compatibleDatabase()
  {
    return {{
      {
        .schema = "public",
        .name = "users",
        .columns =
          {
            {
              .name = "id",
              .type = {.kind = worm::core::ColumnTypeKind::Int64},
              .nullable = false,
              .generated = true,
            },
            {
              .name = "email",
              .type = {.kind = worm::core::ColumnTypeKind::String},
              .nullable = false,
              .unique = true,
            },
          },
        .primaryKey = {"id"},
      },
    }};
  }
} // namespace

int main()
{
  const worm::cli::Invocation invocation{.command = worm::cli::Commands::Check};
  const auto compatible = worm::cli::generator::check(invocation, manifest(), compatibleDatabase());
  const auto compatibleMetrics =
    std::dynamic_pointer_cast<const worm::cli::generator::CheckMetrics>(compatible.metrics);
  if (compatible.status != worm::cli::ExecutionStatus::Success || compatibleMetrics == nullptr ||
      compatibleMetrics->compatibleObjects != 1 || !compatibleMetrics->differences.empty()) {
    std::cerr << "Compatible schemas were not reported correctly.\n";
    return 1;
  }

  auto driftedDatabase = compatibleDatabase();
  driftedDatabase.tables[0].columns[1].nullable = true;
  driftedDatabase.tables[0].columns[1].type.kind = worm::core::ColumnTypeKind::Binary;
  driftedDatabase.tables.push_back({.schema = "public", .name = "audit_log"});
  const auto drifted = worm::cli::generator::check(invocation, manifest(), driftedDatabase);
  const auto driftedMetrics = std::dynamic_pointer_cast<const worm::cli::generator::CheckMetrics>(drifted.metrics);
  if (drifted.status != worm::cli::ExecutionStatus::DriftDetected || driftedMetrics == nullptr ||
      driftedMetrics->incompatibleObjects != 1 || driftedMetrics->missingInCode != 1 ||
      driftedMetrics->differences.size() != 3) {
    std::cerr << "Schema drift was not reported correctly.\n";
    return 1;
  }

  worm::cli::Invocation selected{.command = worm::cli::Commands::Check};
  selected.arguments.entities = {"User"};
  const auto selection = worm::cli::generator::check(selected, manifest(), driftedDatabase);
  const auto selectionMetrics = std::dynamic_pointer_cast<const worm::cli::generator::CheckMetrics>(selection.metrics);
  if (selectionMetrics == nullptr || selectionMetrics->tablesSelected != 1 || selectionMetrics->missingInCode != 0) {
    std::cerr << "Entity selection included unrelated database tables.\n";
    return 1;
  }

  bool rejectedEntity = false;
  selected.arguments.entities = {"Missing"};
  try {
    static_cast<void>(worm::cli::generator::check(selected, manifest(), compatibleDatabase()));
  } catch (const worm::cli::InvalidArgumentException&) {
    rejectedEntity = true;
  }

  bool rejectedTable = false;
  selected.arguments.entities.clear();
  selected.arguments.tables = {"missing"};
  try {
    static_cast<void>(worm::cli::generator::check(selected, manifest(), compatibleDatabase()));
  } catch (const worm::cli::InvalidArgumentException&) {
    rejectedTable = true;
  }

  if (!rejectedEntity || !rejectedTable) {
    std::cerr << "Unknown entity or table selection was accepted.\n";
    return 1;
  }

  return 0;
}
