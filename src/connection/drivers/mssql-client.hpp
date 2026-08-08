#pragma once

#include <connection/client.hpp>
#include <connection/configuration.hpp>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <sql.h>
#include <sqlext.h>

#include <memory>

namespace worm::connection
{
  class MsSqlClient final : public Client
  {
  public:
    explicit MsSqlClient(const ConnectionConfig& databaseConfig);
    ~MsSqlClient() override = default;

    MsSqlClient(const MsSqlClient&) = delete;
    MsSqlClient& operator=(const MsSqlClient&) = delete;

    [[nodiscard]]
    DatabaseType type() const noexcept override;

  private:
    struct EnvironmentDeleter
    {
      using pointer = SQLHENV;

      void operator()(SQLHENV environment) const noexcept;
    };

    struct ConnectionDeleter
    {
      using pointer = SQLHDBC;

      void operator()(SQLHDBC connection) const noexcept;
    };

    std::unique_ptr<void, EnvironmentDeleter> environment_;
    std::unique_ptr<void, ConnectionDeleter> connection_;

    void beginTransactionImpl() override;
    void rollbackTransactionImpl() override;
    void commitTransactionImpl() override;

    [[nodiscard]]
    core::ResultSet executeImpl(const core::Statement& statement) override;
  };
} // namespace worm::connection
