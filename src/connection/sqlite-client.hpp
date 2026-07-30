#pragma once

#include <connection/client.hpp>
#include <connection/configuration.hpp>
#include <sqlite3.h>

#include <memory>

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
    core::ResultSet execute(const core::Statement& statement) override;

    [[nodiscard]]
    DatabaseType type() const noexcept override;

  private:
    struct ConnectionDeleter
    {
      void operator()(sqlite3* connection) const noexcept;
    };

    void throwConnectionError();
    void throwStatementError(sqlite3_stmt* statement) const;

    std::unique_ptr<sqlite3, ConnectionDeleter> connection_;
  };
} // namespace worm::connection
