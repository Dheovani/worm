#include <connection/drivers/sqlite-client.hpp>
#include <connection/schema-inspector.hpp>

#include <sqlite3.h>

#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace
{
  class TemporaryDatabase
  {
  public:
    TemporaryDatabase()
      : path_(std::filesystem::temp_directory_path() / "worm-cli-schema-inspector.db")
    {
      std::filesystem::remove(path_);
      sqlite3* database = nullptr;
      if (sqlite3_open(path_.string().c_str(), &database) != SQLITE_OK) {
        throw std::runtime_error("Could not create the SQLite schema-inspector database.");
      }

      char* message = nullptr;
      const int result = sqlite3_exec(
        database,
        "CREATE TABLE users (id INTEGER PRIMARY KEY, email TEXT NOT NULL UNIQUE, note TEXT NULL, "
        "tenant TEXT NOT NULL, external_id TEXT NOT NULL, UNIQUE (tenant, external_id))",
        nullptr,
        nullptr,
        &message);
      const std::string error = message == nullptr ? "" : message;
      sqlite3_free(message);
      sqlite3_close(database);
      if (result != SQLITE_OK) {
        throw std::runtime_error(error);
      }
    }

    ~TemporaryDatabase()
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
try {
  const TemporaryDatabase database;
  const worm::connection::ConnectionConfig config{.dbname = database.path().string()};
  auto client = std::make_shared<worm::connection::SqliteClient>(config);
  const worm::connection::SchemaInspector inspector{*client};
  const auto schema = inspector.inspect();
  const auto* users = schema.findTable("main", "users");

  if (users == nullptr || users->primaryKey != std::vector<std::string>{"id"} || users->columns.size() != 5 ||
      users->columns[0].nullable || !users->columns[0].generated || users->columns[1].nullable ||
      !users->columns[1].unique || !users->columns[2].nullable || users->columns[3].unique ||
      users->columns[4].unique || users->columns[0].type.kind != worm::core::ColumnTypeKind::Int64 ||
      users->columns[1].type.kind != worm::core::ColumnTypeKind::String) {
    std::cerr << "SQLite schema introspection returned unexpected metadata.\n";
    return 1;
  }

  return 0;
} catch (const std::exception& error) {
  std::cerr << "SQLite schema-inspector contract failed: " << error.what() << '\n';
  return 1;
}
