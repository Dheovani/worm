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
      std::ofstream{manifest_} << R"({"version":1,"entities":[{"name":"User","table":"users","columns":[)"
                               << R"({"name":"id","type":"int64","nullable":false,"generated":true},)"
                               << R"({"name":"email","type":"string","nullable":false,"unique":true}],)"
                               << R"("primaryKey":["id"]}]})";
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
    bool tableExists() const
    {
      sqlite3* database = nullptr;
      if (sqlite3_open(database_.string().c_str(), &database) != SQLITE_OK) {
        throw std::runtime_error("Could not open the CLI push database.");
      }

      sqlite3_stmt* statement = nullptr;
      const int prepared = sqlite3_prepare_v2(
        database, "select 1 from sqlite_master where type = 'table' and name = 'users'", -1, &statement, nullptr);
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
  if (report.status != worm::cli::ExecutionStatus::Success || metrics == nullptr || metrics->plannedTables != 1 ||
      metrics->createdTables != 1 || metrics->failedTables != 0 || !fixture.tableExists()) {
    std::cerr << "End-to-end SQLite push did not create the planned table.\n";
    return 1;
  }

  const auto repeatedReport = worm::cli::generator::push(fixture.invocation());
  const auto repeatedMetrics =
    std::dynamic_pointer_cast<const worm::cli::generator::PushMetrics>(repeatedReport.metrics);
  if (repeatedReport.status != worm::cli::ExecutionStatus::Success || repeatedMetrics == nullptr ||
      repeatedMetrics->compatibleTables != 1 || repeatedMetrics->plannedTables != 0 ||
      repeatedMetrics->createdTables != 0) {
    std::cerr << "Repeated SQLite push did not recognize the generated table as compatible.\n";
    return 1;
  }
  return 0;
} catch (const std::exception& error) {
  std::cerr << "End-to-end SQLite push failed: " << error.what() << '\n';
  return 1;
}
