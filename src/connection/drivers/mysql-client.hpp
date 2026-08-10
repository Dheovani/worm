#pragma once

#include <connection/client.hpp>
#include <connection/configuration.hpp>

#if __has_include(<mysql/mysql.h>)
#include <mysql/mysql.h>
#elif __has_include(<mysql.h>)
#include <mysql.h>
#else
#error "MySQL client headers were not found."
#endif

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
    void beginTransactionImpl() override;
    void rollbackTransactionImpl() override;
    void commitTransactionImpl() override;

    [[nodiscard]]
    core::ResultSet executeImpl(const core::Statement& statement) override;
  };

} // namespace worm::connection
