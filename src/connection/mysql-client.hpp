#pragma once

#include <connection/client.hpp>
#include <connection/configuration.hpp>
#include <mysql/mysql.h>

namespace worm::connection
{
  class MySqlClient final : public Client
  {
  private:
    MYSQL* connection_; // Pointer to the MySQL connection.

    MySqlClient(const char* host, const char* user, const char* passwd, const char* db, unsigned int port);

    // Prevent the use of copy constructor and assignment operator for safety.
    MySqlClient(const MySqlClient&) = delete;
    MySqlClient& operator=(const MySqlClient&) = delete;

  public:
    ~MySqlClient();

    // This static method returns a reference to the Singleton instance of 'MySqlClient'
    // based on the provided database configuration.
    static MySqlClient& getInstance(const ConnectionConfig& databaseConfig);

    // This method is used to execute a database query specific to SQLite.
    core::ResultSet executeQuery(const core::Statement& statement) const override;
  };
} // namespace worm::connection
