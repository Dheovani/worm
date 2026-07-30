#pragma once

#include <connection/client.hpp>
#include <connection/configuration.hpp>
#include <pqxx/pqxx>

#include <memory>
#include <string>

namespace worm::connection
{

  class PgClient final : public Client
  {
  public:
    explicit PgClient(const ConnectionConfig& databaseConfig);
    ~PgClient() override = default;

    PgClient(const PgClient&) = delete;
    PgClient& operator=(const PgClient&) = delete;

    [[nodiscard]]
    core::ResultSet execute(const core::Statement& statement) override;

    [[nodiscard]]
    DatabaseType type() const noexcept override;

  private:
    std::unique_ptr<pqxx::connection> connection_;
    std::unique_ptr<pqxx::work> innerTransaction_;

    void beginTransactionImpl() override;
    void rollbackTransaction() override;
    void commitTransaction() override;
  };

} // namespace worm::connection
