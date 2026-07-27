#pragma once

#include <connection/client.hpp>
#include <connection/configuration.hpp>
#include <pqxx/pqxx>

namespace worm::connection
{
  // PostgreSQL-specific database client.
  class PgClient final : public Client
  {
  private:
    pqxx::connection* connection_; // Pointer to the PostgreSQL connection.

    PgClient(const std::string& data);

    // Prevent the use of copy constructor and assignment operator for safety.
    PgClient(const PgClient&) = delete;
    PgClient& operator=(const PgClient&) = delete;

  public:
    ~PgClient();

    // This static method returns a reference to the Singleton instance of 'PgClient'
    // based on the provided connection data for PostgreSQL.
    static PgClient& getInstance(const ConnectionConfig& connectionData);

    // This method is used to execute a database query specific to PostgreSQL.
    core::ResultSet executeQuery(const std::string& query) const override;
  };
} // namespace worm::connection
