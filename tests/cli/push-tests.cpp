#include <generator/push.hpp>

#include <errors/invalid-cli-argument-exception.hpp>

#include <iostream>
#include <memory>
#include <string>

namespace
{
  worm::cli::generator::SchemaManifest manifest()
  {
    return {
      .entities = {{
        .name = "User",
        .table =
          {
            .schema = "main",
            .name = "users",
            .columns = {{
              .name = "id",
              .type = {.kind = worm::core::ColumnTypeKind::Int64},
              .nullable = false,
              .generated = true,
            }},
            .primaryKey = {"id"},
          },
      }},
    };
  }
} // namespace

int main()
{
  worm::cli::Invocation invocation{.command = worm::cli::Commands::Push};
  const auto schemaManifest = manifest();
  const worm::cli::ExecutionReport missingReport = worm::cli::generator::planPush(invocation, schemaManifest, {});
  const auto missingMetrics = std::dynamic_pointer_cast<const worm::cli::generator::PushMetrics>(missingReport.metrics);
  if (missingReport.status != worm::cli::ExecutionStatus::Success || missingMetrics == nullptr ||
      missingMetrics->entitiesSelected != 1 || missingMetrics->missingTables != 1 ||
      missingMetrics->plannedTables != 1 || missingMetrics->createdTables != 0 ||
      missingReport.info.find("--apply") == std::string::npos) {
    std::cerr << "Push dry-run did not produce a safe missing-table plan.\n";
    return 1;
  }

  const worm::core::SchemaSnapshot compatible{{schemaManifest.entities.front().table}};
  const worm::cli::ExecutionReport compatibleReport =
    worm::cli::generator::planPush(invocation, schemaManifest, compatible);
  const auto compatibleMetrics =
    std::dynamic_pointer_cast<const worm::cli::generator::PushMetrics>(compatibleReport.metrics);
  if (compatibleReport.status != worm::cli::ExecutionStatus::Success || compatibleMetrics == nullptr ||
      compatibleMetrics->compatibleTables != 1 || compatibleMetrics->plannedTables != 0) {
    std::cerr << "Push planning did not recognize a compatible table.\n";
    return 1;
  }

  worm::core::SchemaSnapshot incompatible = compatible;
  incompatible.tables.front().columns.front().nullable = true;
  const auto incompatibleReport = worm::cli::generator::planPush(invocation, schemaManifest, incompatible);
  if (incompatibleReport.status != worm::cli::ExecutionStatus::DriftDetected) {
    std::cerr << "Push planning did not block an incompatible existing table.\n";
    return 1;
  }

  try {
    invocation.arguments.entities = {"Missing"};
    static_cast<void>(worm::cli::generator::planPush(invocation, schemaManifest, {}));
    std::cerr << "Push planning accepted an unknown entity selection.\n";
    return 1;
  } catch (const worm::cli::InvalidCliArgumentException&) {}

  invocation.arguments.entities.clear();
  const worm::cli::generator::SchemaManifest cyclicManifest{
    .entities =
      {
        {
          .name = "Parent",
          .table =
            {
              .schema = "main",
              .name = "parents",
              .columns =
                {
                  {.name = "id", .type = {.kind = worm::core::ColumnTypeKind::Int64}, .nullable = false},
                  {.name = "child_id", .type = {.kind = worm::core::ColumnTypeKind::Int64}, .nullable = false},
                },
              .primaryKey = {"id"},
            },
          .foreignKeys = {{
            .name = "fk_parents_child",
            .columns = {"child_id"},
            .referencedSchema = "main",
            .referencedTable = "children",
            .referencedColumns = {"id"},
          }},
        },
        {
          .name = "Child",
          .table =
            {
              .schema = "main",
              .name = "children",
              .columns =
                {
                  {.name = "id", .type = {.kind = worm::core::ColumnTypeKind::Int64}, .nullable = false},
                  {.name = "parent_id", .type = {.kind = worm::core::ColumnTypeKind::Int64}, .nullable = false},
                },
              .primaryKey = {"id"},
            },
          .foreignKeys = {{
            .name = "fk_children_parent",
            .columns = {"parent_id"},
            .referencedSchema = "main",
            .referencedTable = "parents",
            .referencedColumns = {"id"},
          }},
        },
      },
  };

  try {
    static_cast<void>(worm::cli::generator::planPush(invocation, cyclicManifest, {}));
    std::cerr << "Push planning accepted an inline foreign-key cycle.\n";
    return 1;
  } catch (const worm::cli::InvalidCliArgumentException&) {}

  invocation.arguments.output = "schema.sql";
  invocation.arguments.apply = true;
  try {
    static_cast<void>(worm::cli::generator::push(invocation, schemaManifest));
    std::cerr << "Push accepted simultaneous SQL output and database application.\n";
    return 1;
  } catch (const worm::cli::InvalidCliArgumentException&) {}

  invocation.arguments.apply = false;
  try {
    static_cast<void>(worm::cli::generator::planPush(invocation, schemaManifest, {}));
    std::cerr << "Pure push planning wrote an output file.\n";
    return 1;
  } catch (const worm::cli::InvalidCliArgumentException&) {}

  return 0;
}
