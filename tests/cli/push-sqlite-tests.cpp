#include <generator/push.hpp>

#include <sqlite3.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
  class Fixture
  {
  public:
    Fixture()
      : database_(std::filesystem::temp_directory_path() / "worm-cli-push.db"),
        manifest_(std::filesystem::temp_directory_path() / "worm-cli-push-manifest.json")
    {
      std::filesystem::remove(database_);
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
    bool objectExists(std::string_view type, std::string_view name) const
    {
      sqlite3* database = nullptr;
      if (sqlite3_open(database_.string().c_str(), &database) != SQLITE_OK) {
        throw std::runtime_error("Could not open the CLI push database.");
      }

      sqlite3_stmt* statement = nullptr;
      const int prepared = sqlite3_prepare_v2(
        database, "select 1 from sqlite_master where type = ? and name = ?", -1, &statement, nullptr);
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
  };
} // namespace

int main()
try {
  const Fixture fixture;
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
