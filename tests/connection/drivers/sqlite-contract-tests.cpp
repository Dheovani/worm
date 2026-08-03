#include "driver-contract.hpp"

#include <connection/configuration.hpp>
#include <connection/drivers/sqlite-client.hpp>
#include <core/query/sql-builder.hpp>
#include <sqlite3.h>

#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

namespace
{
  class TemporaryDatabase final
  {
  public:
    TemporaryDatabase()
      : path_(std::filesystem::temp_directory_path() / "worm-sqlite-driver-contract.db")
    {
      std::filesystem::remove(path_);

      sqlite3* connection = nullptr;
      if (sqlite3_open(path_.string().c_str(), &connection) != SQLITE_OK) {
        throw std::runtime_error("Could not open the SQLite contract database.");
      }

      char* errorMessage = nullptr;
      const int result = sqlite3_exec(
        connection,
        "CREATE TABLE worm_driver_contract ("
        "id TEXT PRIMARY KEY, label TEXT NOT NULL, note TEXT NULL)",
        nullptr,
        nullptr,
        &errorMessage);

      if (result != SQLITE_OK) {
        const std::string message = errorMessage != nullptr
          ? errorMessage
          : "Could not create the SQLite contract table.";
        sqlite3_free(errorMessage);
        sqlite3_close(connection);
        throw std::runtime_error(message);
      }

      sqlite3_close(connection);
    }

    ~TemporaryDatabase() noexcept
    {
      std::error_code error;
      std::filesystem::remove(path_, error);
    }

    [[nodiscard]]
    const std::filesystem::path& path() const noexcept
    {
      return path_;
    }

  private:
    std::filesystem::path path_;
  };
} // namespace

int main()
try
{
  const TemporaryDatabase database;
  const worm::connection::ConnectionConfig config{
    .dbname = database.path().string(),
  };

  worm::connection::SqliteClient client{config};
  const worm::core::SqliteBuilder sqlBuilder;

  worm::tests::runDriverContract(
    client,
    sqlBuilder,
    worm::connection::DatabaseType::SQLite);

  return 0;
} catch (const std::exception& error) {
  std::cerr << "SQLite driver contract failed: " << error.what() << "\n";
  return 1;
}
