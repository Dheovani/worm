#pragma once

#include <connection/client.hpp>
#include <sqlite3.h>

namespace worm
{
  namespace connection
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
      // based on the provided database configuration in the form of a 'Json::Value'.
      static SqliteClient& getInstance(const Json::Value& databaseConfig);

      // This method is used to execute a database query specific to SQLite.
      Json::Value executeQuery(const std::string& query) const override;
    };
  } // namespace connection
} // namespace worm
