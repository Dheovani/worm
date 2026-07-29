#pragma once

#include <connection/client.hpp>
#include <connection/configuration.hpp>
#include <mysql/mysql.h>

namespace worm::connection
{
  class MySqlClient final : public Client
  {
  private:
    MYSQL* connection_;

    MySqlClient(const char* host, const char* user, const char* passwd, const char* db, unsigned int port);

    MySqlClient(const MySqlClient&) = delete;
    MySqlClient& operator=(const MySqlClient&) = delete;

  public:
    ~MySqlClient();

    static MySqlClient& getInstance(const ConnectionConfig& databaseConfig);

    core::ResultSet executeQuery(const core::Statement& statement) const override;
  };
} // namespace worm::connection
