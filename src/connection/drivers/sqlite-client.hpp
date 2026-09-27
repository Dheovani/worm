#pragma once

#include <connection/client.hpp>
#include <connection/configuration.hpp>
#include <sqlite3.h>

#include <chrono>
#include <memory>
#include <string_view>

namespace worm::connection
{

  class SqliteClient final : public Client
  {
  public:
    explicit SqliteClient(const ConnectionConfig& databaseConfig);
    ~SqliteClient() override = default;

    SqliteClient(const SqliteClient&) = delete;
    SqliteClient& operator=(const SqliteClient&) = delete;

    [[nodiscard]]
    DatabaseType type() const noexcept override;

  private:
    struct ConnectionDeleter
    {
      void operator()(sqlite3* connection) const noexcept;
    };

    void throwConnectionError();
    void throwStatementError(sqlite3_stmt* statement) const;
    void executeTransactionCommand(const char* sql);

    std::unique_ptr<sqlite3, ConnectionDeleter> connection_;
    int busyTimeoutMilliseconds_{0};
    void beginTransactionImpl() override;
    void rollbackTransactionImpl() override;
    void commitTransactionImpl() override;
    void acquireMigrationLockImpl(std::string_view name, std::chrono::milliseconds timeout) override;
    void releaseMigrationLockImpl(std::string_view name, bool completed) override;

    [[nodiscard]]
    core::ResultSet executeImpl(const core::Statement& statement) override;
  };

} // namespace worm::connection
