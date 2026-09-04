#include <database/diff.hpp>
#include <errors/invalid-cli-argument-exception.hpp>
#include <validator.hpp>

#include <iostream>
#include <memory>
#include <sstream>
#include <string>

namespace
{
  worm::cli::SchemaManifest manifest()
  {
    return {
      .entities =
        {
          {
            .name = "User",
            .table =
              {
                .schema = "public",
                .name = "users",
                .columns =
                  {
                    {.name = "id", .type = {.kind = worm::core::ColumnTypeKind::Int64}, .nullable = false},
                    {
                      .name = "email",
                      .type = {.kind = worm::core::ColumnTypeKind::String},
                      .defaultExpression = "'unknown'",
                      .nullable = false,
                      .unique = true,
                    },
                  },
                .primaryKey = {"id"},
              },
          },
          {
            .name = "Role",
            .table =
              {
                .schema = "public",
                .name = "roles",
                .columns = {{.name = "id", .type = {.kind = worm::core::ColumnTypeKind::Int64}, .nullable = false}},
                .primaryKey = {"id"},
              },
          },
        },
    };
  }

  worm::core::SchemaSnapshot databaseSchema()
  {
    return {{
      {
        .schema = "public",
        .name = "users",
        .columns =
          {
            {.name = "id", .type = {.kind = worm::core::ColumnTypeKind::Int32}, .nullable = false},
            {.name = "email", .type = {.kind = worm::core::ColumnTypeKind::String}},
            {.name = "legacy", .type = {.kind = worm::core::ColumnTypeKind::String}},
          },
        .primaryKey = {"id"},
      },
      {.schema = "public", .name = "audit_log"},
    }};
  }
} // namespace

int main()
{
  const worm::cli::Invocation parsed = worm::cli::parse({"diff"});
  worm::cli::validate(parsed);
  if (parsed.command != worm::cli::Commands::Diff) {
    std::cerr << "Diff command was not parsed.\n";
    return 1;
  }

  try {
    worm::cli::validate({.command = worm::cli::Commands::Diff, .arguments = {.apply = true}});
    std::cerr << "Diff accepted a mutating option.\n";
    return 1;
  } catch (const worm::cli::InvalidCliArgumentException&) {}

  try {
    static_cast<void>(worm::cli::database::diff({.command = worm::cli::Commands::Diff}));
    std::cerr << "Diff accepted a missing manifest.\n";
    return 1;
  } catch (const worm::cli::InvalidCliArgumentException&) {}

  const worm::cli::Invocation invocation{.command = worm::cli::Commands::Diff};
  const worm::cli::ExecutionReport report = worm::cli::database::diff(invocation, manifest(), databaseSchema());
  const auto metrics = std::dynamic_pointer_cast<const worm::cli::database::DiffMetrics>(report.metrics);
  if (report.status != worm::cli::ExecutionStatus::DriftDetected || metrics == nullptr ||
      metrics->entitiesDiscovered != 2 || metrics->tablesDiscovered != 2 || metrics->entitiesCompared != 2 ||
      metrics->differencesDetected != 7 || metrics->missingTables != 1 || metrics->unexpectedTables != 1 ||
      metrics->unexpectedColumns != 1 || metrics->metadataMismatches != 4) {
    std::cerr << "Diff metrics did not classify schema differences.\n";
    return 1;
  }

  std::ostringstream text;
  worm::cli::outputReport(report, "text", text);
  if (text.str().find("[column-type-mismatch] public.users.id") == std::string::npos ||
      text.str().find("[missing-table] public.roles") == std::string::npos ||
      text.str().find("[unexpected-table] public.audit_log") == std::string::npos) {
    std::cerr << "Diff text output omitted structured differences.\n";
    return 1;
  }

  std::ostringstream json;
  worm::cli::outputReport(report, "json", json);
  if (json.str().find("\"kind\":\"column-type-mismatch\"") == std::string::npos ||
      json.str().find("\"target\":\"public.users.id\"") == std::string::npos) {
    std::cerr << "Diff JSON output omitted structured differences.\n";
    return 1;
  }

  worm::core::SchemaSnapshot compatible{{manifest().entities[0].table, manifest().entities[1].table}};
  const worm::cli::ExecutionReport matching = worm::cli::database::diff(invocation, manifest(), compatible);
  if (matching.status != worm::cli::ExecutionStatus::Success) {
    std::cerr << "Diff reported differences for compatible schemas.\n";
    return 1;
  }

  return 0;
}
