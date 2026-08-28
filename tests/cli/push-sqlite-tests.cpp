#include <generator/push.hpp>

#include <errors/worm-cli-exception.hpp>

#include <sqlite3.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace
{
  class Fixture
  {
  public:
    Fixture()
      : database_(std::filesystem::temp_directory_path() / "worm-cli-push.db"),
        manifest_(std::filesystem::temp_directory_path() / "worm-cli-push-manifest.json"),
        sqlOutput_(std::filesystem::temp_directory_path() / "worm-cli-push.sql")
    {
      std::filesystem::remove(database_);
      std::filesystem::remove(sqlOutput_);
      std::ofstream{manifest_}
        << R"({"version":1,"entities":[{"name":"User","table":"users","columns":[)"
        << R"({"name":"id","type":"int64","nullable":false,"generated":true},)"
        << R"({"name":"role_id","type":"int64","nullable":false},)"
        << R"({"name":"email","type":"string","nullable":false,"unique":true}],)"
        << R"("primaryKey":["id"],"indexes":[{"name":"idx_users_email","columns":["email"]}],)"
        << R"("foreignKeys":[{"name":"fk_users_role","columns":["role_id"],"referencedTable":"roles","referencedColumns":["id"],"onDelete":"restrict"}]},)"
        << R"({"name":"Role","table":"roles","columns":[{"name":"id","type":"int64","nullable":false,"generated":true}],"primaryKey":["id"]}]})";
    }

    ~Fixture()
    {
      std::error_code error;
      std::filesystem::remove(database_, error);
      std::filesystem::remove(manifest_, error);
      std::filesystem::remove(sqlOutput_, error);
    }

    [[nodiscard]]
    worm::cli::Invocation invocation() const
    {
      worm::cli::Invocation value{.command = worm::cli::Commands::Push};
      value.global.manifest = manifest_.string();
      value.global.driver = "sqlite";
      value.global.database = database_.string();
      value.arguments.apply = true;
      return value;
    }

    [[nodiscard]]
    worm::cli::Invocation sqlInvocation() const
    {
      worm::cli::Invocation value = invocation();
      value.arguments.apply = false;
      value.arguments.output = sqlOutput_.string();
      return value;
    }

    [[nodiscard]]
    const std::filesystem::path& sqlOutput() const noexcept
    {
      return sqlOutput_;
    }

    [[nodiscard]]
    bool objectExists(std::string_view type, std::string_view name) const
    {
      sqlite3* database = nullptr;
      if (sqlite3_open(database_.string().c_str(), &database) != SQLITE_OK) {
        throw std::runtime_error("Could not open the CLI push database.");
      }

      sqlite3_stmt* statement = nullptr;
      const int prepared = sqlite3_prepare_v2(
        database,
        "select 1 from sqlite_master where type = ? and name = ?",
        -1,
        &statement,
        nullptr);
      if (prepared == SQLITE_OK) {
        sqlite3_bind_text(statement, 1, type.data(), static_cast<int>(type.size()), SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 2, name.data(), static_cast<int>(name.size()), SQLITE_TRANSIENT);
      }
      const bool exists = prepared == SQLITE_OK && sqlite3_step(statement) == SQLITE_ROW;
      sqlite3_finalize(statement);
      sqlite3_close(database);
      return exists;
    }

  private:
    std::filesystem::path database_;
    std::filesystem::path manifest_;
    std::filesystem::path sqlOutput_;
  };
} // namespace

int main()
try {
  const Fixture fixture;
  const auto sqlReport = worm::cli::generator::push(fixture.sqlInvocation());
  const auto sqlMetrics = std::dynamic_pointer_cast<const worm::cli::generator::PushMetrics>(sqlReport.metrics);
  std::ifstream sqlStream{fixture.sqlOutput()};
  std::ostringstream sqlContents;
  sqlContents << sqlStream.rdbuf();
  const std::size_t rolesPosition = sqlContents.str().find("create table \"main\".\"roles\"");
  const std::size_t usersPosition = sqlContents.str().find("create table \"main\".\"users\"");
  if (sqlReport.status != worm::cli::ExecutionStatus::Success || sqlMetrics == nullptr ||
      sqlMetrics->generatedStatements != 3 || sqlMetrics->generatedSqlFiles != 1 || sqlMetrics->createdTables != 0 ||
      !sqlStream || rolesPosition == std::string::npos || usersPosition == std::string::npos ||
      rolesPosition >= usersPosition ||
      sqlContents.str().find("create index \"main\".\"idx_users_email\"") == std::string::npos ||
      fixture.objectExists("table", "users")) {
    std::cerr << "SQLite push did not generate a reviewable SQL file without applying it.\n";
    return 1;
  }

  try {
    static_cast<void>(worm::cli::generator::push(fixture.sqlInvocation()));
    std::cerr << "SQLite push overwrote an existing SQL output file.\n";
    return 1;
  } catch (const worm::cli::WormCliException&) {}

  const auto report = worm::cli::generator::push(fixture.invocation());
  const auto metrics = std::dynamic_pointer_cast<const worm::cli::generator::PushMetrics>(report.metrics);
  if (report.status != worm::cli::ExecutionStatus::Success || metrics == nullptr || metrics->plannedTables != 2 ||
      metrics->createdTables != 2 || metrics->failedTables != 0 || !fixture.objectExists("table", "users") ||
      !fixture.objectExists("table", "roles") || !fixture.objectExists("index", "idx_users_email")) {
    std::cerr << "End-to-end SQLite push did not create the planned schema: " << report.info << '\n';
    if (metrics != nullptr) {
      std::cerr << "planned=" << metrics->plannedTables << ", created=" << metrics->createdTables
                << ", failed=" << metrics->failedTables << '\n';
    }
    return 1;
  }

  const auto repeatedReport = worm::cli::generator::push(fixture.invocation());
  const auto repeatedMetrics =
    std::dynamic_pointer_cast<const worm::cli::generator::PushMetrics>(repeatedReport.metrics);
  if (repeatedReport.status != worm::cli::ExecutionStatus::Success || repeatedMetrics == nullptr ||
      repeatedMetrics->compatibleTables != 2 || repeatedMetrics->plannedTables != 0 ||
      repeatedMetrics->createdTables != 0) {
    std::cerr << "Repeated SQLite push did not recognize the generated table as compatible.\n";
    return 1;
  }
  return 0;
} catch (const std::exception& error) {
  std::cerr << "End-to-end SQLite push failed: " << error.what() << '\n';
  return 1;
}
