#include <database/diff.hpp>

#include <sqlite3.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace
{
  class Fixture
  {
  public:
    Fixture()
      : database_(std::filesystem::temp_directory_path() / "worm-cli-diff.db"),
        manifest_(std::filesystem::temp_directory_path() / "worm-cli-diff-manifest.json")
    {
      std::filesystem::remove(database_);
      execute("CREATE TABLE users (id INTEGER PRIMARY KEY, email TEXT NOT NULL UNIQUE)");
      std::ofstream{manifest_}
        << R"({"version":1,"entities":[{"name":"User","table":"users","columns":[)"
        << R"({"name":"id","nullable":false,"generated":true},{"name":"email","nullable":false,"unique":true}],)"
        << R"("primaryKey":["id"]}]})";
    }

    ~Fixture()
    {
      std::error_code error;
      std::filesystem::remove(database_, error);
      std::filesystem::remove(manifest_, error);
    }

    void execute(const char* sql) const
    {
      sqlite3* database = nullptr;
      if (sqlite3_open(database_.string().c_str(), &database) != SQLITE_OK) {
        throw std::runtime_error("Could not open the CLI diff database.");
      }
      char* message = nullptr;
      const int result = sqlite3_exec(database, sql, nullptr, nullptr, &message);
      const std::string error = message == nullptr ? "" : message;
      sqlite3_free(message);
      sqlite3_close(database);
      if (result != SQLITE_OK) {
        throw std::runtime_error(error);
      }
    }

    [[nodiscard]]
    worm::cli::Invocation invocation() const
    {
      worm::cli::Invocation value{.command = worm::cli::Commands::Diff};
      value.global.manifest = manifest_.string();
      value.global.driver = "sqlite";
      value.global.database = database_.string();
      return value;
    }

  private:
    std::filesystem::path database_;
    std::filesystem::path manifest_;
  };
} // namespace

int main()
try {
  const Fixture fixture;
  const worm::cli::ExecutionReport compatible = worm::cli::database::diff(fixture.invocation());
  if (compatible.status != worm::cli::ExecutionStatus::Success) {
    std::cerr << "SQLite diff rejected a compatible schema.\n";
    return 1;
  }

  fixture.execute("ALTER TABLE users ADD COLUMN legacy TEXT");
  const worm::cli::ExecutionReport drifted = worm::cli::database::diff(fixture.invocation());
  const auto metrics = std::dynamic_pointer_cast<const worm::cli::database::DiffMetrics>(drifted.metrics);
  if (drifted.status != worm::cli::ExecutionStatus::DriftDetected || metrics == nullptr ||
      metrics->unexpectedColumns != 1 || metrics->differencesDetected != 1) {
    std::cerr << "SQLite diff did not report database drift.\n";
    return 1;
  }
  return 0;
} catch (const std::exception& error) {
  std::cerr << "End-to-end SQLite diff failed: " << error.what() << '\n';
  return 1;
}
