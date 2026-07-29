#pragma once

#include <connection/client.hpp>
#include <connection/configuration.hpp>
#include <sqlite3.h>

namespace worm::connection
{
  class SqliteClient final : public Client
  {
  private:
    enum class ErrorHandlingAction
    {
      CloseConnection,
      FinalizeStatement
    };

    sqlite3* connection_;

    explicit SqliteClient(const char* databaseName);

    SqliteClient(const SqliteClient&) = delete;
    SqliteClient& operator=(const SqliteClient&) = delete;

    void handleError(ErrorHandlingAction action, sqlite3_stmt* statement = nullptr) const;

  public:
    ~SqliteClient();

    static SqliteClient& getInstance(const ConnectionConfig& databaseConfig);

    core::ResultSet executeQuery(const core::Statement& statement) const override;
  };
} // namespace worm::connection
