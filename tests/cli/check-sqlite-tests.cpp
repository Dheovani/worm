#include <generator/check.hpp>

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
      : database_(std::filesystem::temp_directory_path() / "worm-cli-check.db"),
        manifest_(std::filesystem::temp_directory_path() / "worm-cli-check-manifest.json")
    {
      std::filesystem::remove(database_);
      sqlite3* database = nullptr;
      if (sqlite3_open(database_.string().c_str(), &database) != SQLITE_OK) {
        throw std::runtime_error("Could not create the CLI check database.");
      }

      char* message = nullptr;
      const int result = sqlite3_exec(database,
        "CREATE TABLE users (id INTEGER PRIMARY KEY, email TEXT NOT NULL UNIQUE)",
        nullptr,
        nullptr,
        &message);
      const std::string error = message == nullptr ? "" : message;
      sqlite3_free(message);
      sqlite3_close(database);
      if (result != SQLITE_OK) {
        throw std::runtime_error(error);
      }

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

    [[nodiscard]]
    worm::cli::Invocation invocation() const
    {
      worm::cli::Invocation value{.command = worm::cli::Commands::Check};
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
  const auto report = worm::cli::generator::check(fixture.invocation());
  const auto metrics = std::dynamic_pointer_cast<const worm::cli::generator::CheckMetrics>(report.metrics);
  if (report.status != worm::cli::ExecutionStatus::Success || metrics == nullptr || metrics->compatibleObjects != 1 ||
      !metrics->differences.empty()) {
    std::cerr << "End-to-end SQLite check did not report compatible schemas.\n";
    return 1;
  }
  return 0;
} catch (const std::exception& error) {
  std::cerr << "End-to-end SQLite check failed: " << error.what() << '\n';
  return 1;
}
