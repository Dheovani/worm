#include <database/seed.hpp>
#include <parser.hpp>
#include <validator.hpp>

#include <sqlite3.h>

#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace
{
  class SqliteFixture
  {
  public:
    explicit SqliteFixture(std::string name)
      : path_(std::filesystem::temp_directory_path() / std::move(name))
    {
      std::error_code error;
      std::filesystem::remove(path_, error);
      if (sqlite3_open(path_.string().c_str(), &database_) != SQLITE_OK) {
        throw std::runtime_error("Unable to open the SQLite seed test database.");
      }
      execute("PRAGMA foreign_keys = ON");
    }

    ~SqliteFixture()
    {
      sqlite3_close(database_);
      std::error_code error;
      std::filesystem::remove(path_, error);
    }

    void execute(const char* sql) const
    {
      char* error = nullptr;
      if (sqlite3_exec(database_, sql, nullptr, nullptr, &error) != SQLITE_OK) {
        const std::string message = error == nullptr ? "SQLite statement failed." : error;
        sqlite3_free(error);
        throw std::runtime_error(message);
      }
    }

    [[nodiscard]]
    int count(std::string_view table) const
    {
      const std::string sql = "SELECT COUNT(*) FROM " + std::string{table};
      sqlite3_stmt* statement = nullptr;
      if (sqlite3_prepare_v2(database_, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK ||
          sqlite3_step(statement) != SQLITE_ROW) {
        sqlite3_finalize(statement);
        throw std::runtime_error("Unable to count SQLite seed rows.");
      }
      const int result = sqlite3_column_int(statement, 0);
      sqlite3_finalize(statement);
      return result;
    }

    [[nodiscard]]
    worm::cli::Invocation invocation(std::size_t rows) const
    {
      worm::cli::Invocation result = worm::cli::parse(
        {"--driver",
          "sqlite",
          "--database",
          path_.string(),
          "seed",
          "--all",
          "--rows",
          std::to_string(rows),
          "--apply"});
      worm::cli::validate(result);
      return result;
    }

  private:
    std::filesystem::path path_;
    sqlite3* database_{nullptr};
  };
} // namespace

int main()
{
  {
    const SqliteFixture fixture{"worm-cli-seed-success.sqlite"};
    fixture.execute("CREATE TABLE roles (id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE)");
    fixture.execute(
      "CREATE TABLE users (id INTEGER PRIMARY KEY, role_id INTEGER NOT NULL REFERENCES roles(id), "
      "name TEXT NOT NULL UNIQUE)");

    const worm::cli::ExecutionReport report = worm::cli::database::seed(fixture.invocation(3));
    const auto metrics = std::dynamic_pointer_cast<const worm::cli::database::SeedMetrics>(report.metrics);
    if (report.status != worm::cli::ExecutionStatus::Success || metrics == nullptr || metrics->plannedRows != 6 ||
        metrics->generatedStatements != 6 || metrics->executedStatements != 6 || metrics->insertedRows != 6 ||
        fixture.count("roles") != 3 || fixture.count("users") != 3) {
      std::cerr << "SQLite seed did not insert all planned rows and foreign-key values.\n";
      return 1;
    }
  }

  {
    const SqliteFixture fixture{"worm-cli-seed-rollback.sqlite"};
    fixture.execute("CREATE TABLE limited (id INTEGER PRIMARY KEY CHECK (id < 2))");

    const worm::cli::ExecutionReport report = worm::cli::database::seed(fixture.invocation(2));
    const auto metrics = std::dynamic_pointer_cast<const worm::cli::database::SeedMetrics>(report.metrics);
    if (report.status != worm::cli::ExecutionStatus::Failed || metrics == nullptr ||
        metrics->generatedStatements != 2 || metrics->executedStatements != 1 || metrics->insertedRows != 0 ||
        fixture.count("limited") != 0) {
      std::cerr << "SQLite seed did not roll back its transaction after an insert failure.\n";
      return 1;
    }
  }

  return 0;
}
