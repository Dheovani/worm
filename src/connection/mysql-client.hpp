#pragma once

#include <connection/client.hpp>
#include <connection/configuration.hpp>
#include <mysql/mysql.h>

#include <memory>

namespace worm::connection
{
  class MySqlClient final : public Client
  {
  public:
    explicit MySqlClient(const ConnectionConfig& databaseConfig);
    ~MySqlClient() override = default;

    MySqlClient(const MySqlClient&) = delete;
    MySqlClient& operator=(const MySqlClient&) = delete;

    [[nodiscard]]
    core::ResultSet execute(const core::Statement& statement) override;

    [[nodiscard]]
    DatabaseType type() const noexcept override;

  private:
    std::unique_ptr<MYSQL, decltype(&mysql_close)> connection_;
  };
} // namespace worm::connection
