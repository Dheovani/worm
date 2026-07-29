#pragma once

#include <connection/client.hpp>
#include <connection/configuration.hpp>
#include <pqxx/pqxx>

namespace worm::connection
{
  class PgClient final : public Client
  {
  private:
    pqxx::connection* connection_;

    PgClient(const std::string& data);

    PgClient(const PgClient&) = delete;
    PgClient& operator=(const PgClient&) = delete;

  public:
    ~PgClient();

    static PgClient& getInstance(const ConnectionConfig& connectionData);

    core::ResultSet executeQuery(const core::Statement& statement) const override;
  };
} // namespace worm::connection
