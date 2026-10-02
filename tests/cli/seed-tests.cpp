#include <database/seed.hpp>
#include <errors/invalid-cli-argument-exception.hpp>
#include <parser.hpp>
#include <validator.hpp>

#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace
{
  bool rejects(const worm::cli::Invocation& invocation)
  {
    try {
      worm::cli::validate(invocation);
    } catch (const worm::cli::InvalidCliArgumentException&) {
      return true;
    }
    return false;
  }

  worm::core::SchemaTableSnapshot table(
    std::string name,
    std::vector<std::string> foreignKeys = {},
    std::vector<std::string> foreignKeyColumns = {})
  {
    worm::core::SchemaTableSnapshot result{
      .schema = "public",
      .name = std::move(name),
      .columns = {{
        .name = "id",
        .type = {.kind = worm::core::ColumnTypeKind::Int64},
        .nullable = false,
      }},
      .primaryKey = {"id"},
      .foreignKey = std::move(foreignKeys),
    };

    for (std::string& column : foreignKeyColumns) {
      result.columns.push_back({
        .name = std::move(column),
        .type = {.kind = worm::core::ColumnTypeKind::Int64},
        .nullable = false,
      });
    }
    return result;
  }

  bool rejectsSeed(const worm::cli::Invocation& invocation, const worm::core::SchemaSnapshot& schema)
  {
    try {
      static_cast<void>(worm::cli::database::seed(invocation, schema));
    } catch (const worm::cli::InvalidCliArgumentException&) {
      return true;
    }
    return false;
  }
} // namespace

int main()
{
  using worm::cli::Commands;
  using worm::cli::Invocation;

  const Invocation parsed = worm::cli::parse({"seed", "--all", "--rows", "2"});
  if (parsed.command != Commands::Seed || !parsed.arguments.all || parsed.arguments.rows != "2") {
    std::cerr << "Seed command options were not parsed correctly.\n";
    return 1;
  }
  worm::cli::validate(parsed);

  Invocation conflicting = parsed;
  conflicting.arguments.tables = {"users"};
  Invocation missingTarget{.command = Commands::Seed, .arguments = {.apply = true}};
  Invocation misplaced{.command = Commands::Push, .arguments = {.rows = "2"}};
  if (!rejects(conflicting) || !rejects(missingTarget) || !rejects(misplaced)) {
    std::cerr << "Seed command validation accepted conflicting or misplaced options.\n";
    return 1;
  }

  const worm::core::SchemaSnapshot schema{{
    table("roles"),
    table("users", {"role_id -> roles.id"}, {"role_id"}),
    table("posts", {"user_id -> users.id"}, {"user_id"}),
  }};
  Invocation selected{.command = Commands::Seed, .arguments = {.tables = {"posts"}}};
  const worm::cli::ExecutionReport report = worm::cli::database::seed(selected, schema);
  if (report.metrics == nullptr) {
    std::cerr << "Seed planning did not return its execution metrics.\n";
    return 1;
  }

  Invocation unknown{.command = Commands::Seed, .arguments = {.tables = {"missing"}}};
  try {
    static_cast<void>(worm::cli::database::seed(unknown, schema));
    std::cerr << "Seed planning accepted an unknown table.\n";
    return 1;
  } catch (const worm::cli::InvalidCliArgumentException&) {}

  const worm::core::SchemaSnapshot cyclic{{
    table("parents", {"child_id -> children.id"}),
    table("children", {"parent_id -> parents.id"}),
  }};
  Invocation cycle{.command = Commands::Seed, .arguments = {.all = true}};
  try {
    static_cast<void>(worm::cli::database::seed(cycle, cyclic));
    std::cerr << "Seed planning accepted a foreign-key dependency cycle.\n";
    return 1;
  } catch (const worm::cli::InvalidCliArgumentException&) {}

  worm::core::SchemaTableSnapshot invalidRequired = table("invalid_required");
  invalidRequired.columns.push_back({
    .name = "unsupported",
    .type = {.kind = worm::core::ColumnTypeKind::Unknown},
    .nullable = false,
  });
  const worm::core::SchemaSnapshot invalidConstraint{{std::move(invalidRequired)}};
  if (!rejectsSeed(Invocation{.command = Commands::Seed, .arguments = {.all = true}}, invalidConstraint)) {
    std::cerr << "Seed planning accepted a required column without a generated value.\n";
    return 1;
  }

  worm::core::SchemaTableSnapshot stringParent = table("string_parent");
  stringParent.columns[0].type.kind = worm::core::ColumnTypeKind::String;
  const worm::core::SchemaSnapshot incompatibleForeignKey{{
    std::move(stringParent),
    table("numeric_child", {"parent_id -> string_parent.id"}, {"parent_id"}),
  }};
  if (!rejectsSeed(Invocation{.command = Commands::Seed, .arguments = {.all = true}}, incompatibleForeignKey)) {
    std::cerr << "Seed planning accepted an incompatible foreign-key value.\n";
    return 1;
  }

  return 0;
}
