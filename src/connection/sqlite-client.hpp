#pragma once

#include <connection/client.hpp>
#include <connection/configuration.hpp>
#include <sqlite3.h>

namespace worm::connection
{
  // SQLite-specific database client.
  class SqliteClient final : public Client
  {
  private:
    enum class ErrorHandlingAction
    {
      CloseConnection,
      FinalizeStatement
    };

    sqlite3* connection_; // Pointer to the SQLite connection.

    explicit SqliteClient(const char* databaseName);

    // Prevent the use of copy constructor and assignment operator for safety.
    SqliteClient(const SqliteClient&) = delete;
    SqliteClient& operator=(const SqliteClient&) = delete;

    // Deals with possible errors according to the action defined by the user
    void handleError(ErrorHandlingAction action, sqlite3_stmt* statement = nullptr) const;

  public:
    ~SqliteClient();

    // This static method returns a reference to the singleton SqliteClient instance.
    // based on the provided database configuration.
    static SqliteClient& getInstance(const ConnectionConfig& databaseConfig);

    // This method is used to execute a database query specific to SQLite.
    core::ResultSet executeQuery(const core::Statement& statement) const override;
  };
} // namespace worm::connection
