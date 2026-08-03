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
    DatabaseType type() const noexcept override;

  private:
    std::unique_ptr<MYSQL, decltype(&mysql_close)> connection_;
    bool transactionActive_{false};

    void beginTransactionImpl() override;
    void rollbackTransaction() override;
    void commitTransaction() override;

    [[nodiscard]]
    core::ResultSet execute(const core::Statement& statement) override;
  };

} // namespace worm::connection
