#pragma once

#include <sqlite3.h>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

namespace worm::examples
{
  class SqliteExampleDatabase final
  {
  public:
    SqliteExampleDatabase(std::string_view filename, std::string_view schema)
      : path_(std::filesystem::temp_directory_path() / filename)
    {
      std::filesystem::remove(path_);

      if (sqlite3_open(path_.string().c_str(), &connection_) != SQLITE_OK) {
        const std::string message =
          connection_ != nullptr ? sqlite3_errmsg(connection_) : "Could not create the example SQLite database.";
        close();

        std::error_code error;
        std::filesystem::remove(path_, error);
        throw std::runtime_error(message);
      }

      try {
        execute(schema);
      } catch (...) {
        close();

        std::error_code error;
        std::filesystem::remove(path_, error);
        throw;
      }
    }

    ~SqliteExampleDatabase() noexcept
    {
      close();

      std::error_code error;
      std::filesystem::remove(path_, error);
    }

    SqliteExampleDatabase(const SqliteExampleDatabase&) = delete;
    SqliteExampleDatabase& operator=(const SqliteExampleDatabase&) = delete;
    SqliteExampleDatabase(SqliteExampleDatabase&&) = delete;
    SqliteExampleDatabase& operator=(SqliteExampleDatabase&&) = delete;

    [[nodiscard]]
    const std::filesystem::path& path() const noexcept
    {
      return path_;
    }

  private:
    void execute(std::string_view sql)
    {
      char* errorMessage = nullptr;
      const std::string ownedSql{sql};
      const int result = sqlite3_exec(connection_, ownedSql.c_str(), nullptr, nullptr, &errorMessage);

      if (result == SQLITE_OK) {
        return;
      }

      const std::string message = errorMessage != nullptr ? errorMessage : "Could not prepare the example schema.";
      sqlite3_free(errorMessage);
      throw std::runtime_error(message);
    }

    void close() noexcept
    {
      if (connection_ != nullptr) {
        sqlite3_close(connection_);
        connection_ = nullptr;
      }
    }

    std::filesystem::path path_;
    sqlite3* connection_ = nullptr;
  };
} // namespace worm::examples
