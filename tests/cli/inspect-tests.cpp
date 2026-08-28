#include <database/inspect.hpp>
#include <errors/invalid-cli-argument-exception.hpp>
#include <validator.hpp>

#include <core/model/schema-snapshot.hpp>

#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

namespace
{
  [[nodiscard]]
  bool rejectsArguments(worm::cli::CommandArguments arguments)
  {
    try {
      worm::cli::validate({.command = worm::cli::Commands::Inspect, .arguments = std::move(arguments)});
    } catch (const worm::cli::InvalidCliArgumentException&) {
      return true;
    }
    return false;
  }
} // namespace

int main()
{
  const worm::cli::Invocation parsed = worm::cli::parse({"inspect"});
  worm::cli::validate(parsed);
  if (parsed.command != worm::cli::Commands::Inspect || !rejectsArguments({.entities = {"User"}}) ||
      !rejectsArguments({.output = "schema.json"}) || !rejectsArguments({.query = "SELECT 1"})) {
    std::cerr << "Inspect argument contract failed.\n";
    return 1;
  }

  const worm::core::SchemaSnapshot snapshot{{
    {
      .schema = "sales",
      .name = "orders",
      .columns = {{.name = "id", .type = {.kind = worm::core::ColumnTypeKind::Int64}, .nullable = false}},
      .primaryKey = {"id"},
    },
    {
      .schema = "public",
      .name = "users",
      .columns =
        {
          {
            .name = "id",
            .type = {.kind = worm::core::ColumnTypeKind::Int64, .nativeName = "bigint"},
            .nullable = false,
            .generated = true,
          },
          {
            .name = "email",
            .type = {.kind = worm::core::ColumnTypeKind::String, .nativeName = "varchar(255)", .length = 255},
            .nullable = false,
            .unique = true,
          },
        },
      .primaryKey = {"id"},
      .foreignKey = {"role_id -> roles.id"},
      .indexes = {"users_email_idx (email) UNIQUE"},
    },
    {
      .schema = "public",
      .name = "roles",
      .columns = {{.name = "id", .type = {.kind = worm::core::ColumnTypeKind::Int32}, .nullable = false}},
      .primaryKey = {"id"},
    },
  }};

  worm::cli::Invocation textInvocation{.command = worm::cli::Commands::Inspect};
  const worm::cli::ExecutionReport textReport = worm::cli::database::inspect(textInvocation, snapshot);
  std::ostringstream text;
  worm::cli::outputReport(textReport, "text", text);
  const std::string textOutput = text.str();

  const auto metrics = std::dynamic_pointer_cast<const worm::cli::database::InspectMetrics>(textReport.metrics);
  const std::size_t rolesPosition = textOutput.find("  Table: roles");
  const std::size_t usersPosition = textOutput.find("  Table: users");
  if (metrics == nullptr || metrics->schemasDiscovered != 2 || metrics->tablesDiscovered != 3 ||
      metrics->columnsDiscovered != 4 || metrics->primaryKeysDiscovered != 3 || metrics->foreignKeysDiscovered != 1 ||
      metrics->indexesDiscovered != 1 || textOutput.find("Schema: public") == std::string::npos ||
      textOutput.find("varchar(255)  NOT NULL UNIQUE") == std::string::npos ||
      textOutput.find("role_id -> roles.id") == std::string::npos || rolesPosition == std::string::npos ||
      usersPosition == std::string::npos || rolesPosition >= usersPosition) {
    std::cerr << "Inspect text translation failed.\n";
    return 1;
  }

  worm::cli::Invocation jsonInvocation{.global = {.format = "json"}, .command = worm::cli::Commands::Inspect};
  const worm::cli::ExecutionReport jsonReport = worm::cli::database::inspect(jsonInvocation, snapshot);
  std::ostringstream json;
  worm::cli::outputReport(jsonReport, "json", json);

  const std::string expectedJson =
    R"json({"schemas":[{"name":"public","tables":[{"name":"roles","columns":[{"name":"id","type":"int32","nullable":false,"generated":false,"unique":false}],"primaryKey":["id"],"foreignKeys":[],"indexes":[]},{"name":"users","columns":[{"name":"id","type":"bigint","nullable":false,"generated":true,"unique":false},{"name":"email","type":"varchar(255)","nullable":false,"generated":false,"unique":true}],"primaryKey":["id"],"foreignKeys":["role_id -> roles.id"],"indexes":["users_email_idx (email) UNIQUE"]}]},{"name":"sales","tables":[{"name":"orders","columns":[{"name":"id","type":"int64","nullable":false,"generated":false,"unique":false}],"primaryKey":["id"],"foreignKeys":[],"indexes":[]}]}]})json"
    "\n";
  if (json.str() != expectedJson) {
    std::cerr << "Inspect JSON translation failed.\nExpected: " << expectedJson << "Actual: " << json.str();
    return 1;
  }

  return 0;
}
