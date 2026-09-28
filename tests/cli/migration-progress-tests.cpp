#include <helpers/migration/migration-progress.hpp>

#include <core/model/migration-artifact.hpp>
#include <core/output/result-set.hpp>
#include <core/query/statement.hpp>

#include <cstdint>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>

int main()
{
  const worm::core::MigrationArtifact artifact = worm::core::makeMigrationArtifact(
    "20260928180000",
    "create-users",
    "sqlite",
    {
      {
        .description = "Create users",
        .sql = "create table users (id integer primary key)",
        .risk = worm::core::MigrationRisk::Safe,
      },
    });
  const worm::core::Statement statement{
    "update users set name = ? where id = ?",
    {std::string{"Ada"}, std::int64_t{1}},
  };

  std::ostringstream text;
  worm::cli::migration::MigrationProgressLogger textLogger{&text, false};
  textLogger.migrationStarted(artifact);
  textLogger.queryExecuted(
    "Update user",
    statement,
    worm::core::ResultSet{std::uint64_t{1}},
    worm::core::MigrationRisk::Ambiguous);
  textLogger.migrationCompleted(artifact);
  if (textLogger.queriesExecuted() != 1 || textLogger.statementsExecuted() != 1 || textLogger.affectedRows() != 1 ||
      text.str().find("[migrate] Applying 20260928180000") == std::string::npos ||
      text.str().find("Risk: ambiguous") == std::string::npos ||
      text.str().find("SQL: update users set name = ? where id = ?") == std::string::npos ||
      text.str().find("Parameters: 2") == std::string::npos ||
      text.str().find("Affected rows: 1") == std::string::npos ||
      text.str().find("[migrate] Applied 20260928180000") == std::string::npos ||
      text.str().find("Ada") != std::string::npos) {
    std::cerr << "Migration progress logger did not emit safe real-time text diagnostics.\n";
    return 1;
  }

  std::ostringstream json;
  worm::cli::migration::MigrationProgressLogger jsonLogger{&json, true};
  jsonLogger.queryExecuted("Load history", worm::core::Statement{"select 1"}, worm::core::ResultSet{}, std::nullopt);
  jsonLogger.migrationFailed(artifact, "DDL failed");
  if (json.str().find("{\"event\":\"migration-query-completed\"") == std::string::npos ||
      json.str().find("\"risk\":null") == std::string::npos ||
      json.str().find("\"returnedRows\":0") == std::string::npos ||
      json.str().find("{\"event\":\"migration-failed\"") == std::string::npos ||
      json.str().find("\"reason\":\"DDL failed\"") == std::string::npos) {
    std::cerr << "Migration progress logger did not emit structured real-time diagnostics.\n";
    return 1;
  }

  return 0;
}
