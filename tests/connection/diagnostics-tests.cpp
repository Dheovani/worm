#include <connection/client.hpp>
#include <connection/diagnostics.hpp>
#include <core/query/statement.hpp>
#include <errors/query-execution-exception.hpp>

#include <iostream>
#include <string>
#include <vector>

namespace
{
  class DiagnosticClient final : public worm::connection::Client
  {
  public:
    explicit DiagnosticClient(bool fail = false)
      : fail_(fail)
    {}

    worm::connection::DatabaseType type() const noexcept override
    {
      return worm::connection::DatabaseType::SQLite;
    }

    std::vector<std::string> statements;

  private:
    worm::core::ResultSet executeImpl(const worm::core::Statement& statement) override
    {
      statements.push_back(statement.sql);
      if (fail_) {
        throw worm::QueryExecutionException("Connection unavailable.");
      }
      return worm::core::ResultSet{{worm::core::ResultRow{{{"value", std::int64_t{1}}}}}};
    }

    void beginTransactionImpl() override {}
    void rollbackTransactionImpl() override {}
    void commitTransactionImpl() override {}

    bool fail_{};
  };
} // namespace

int main()
{
  const worm::connection::DatabaseVersion postgres = worm::connection::parseDatabaseVersion("17.4 (Ubuntu)");
  const worm::connection::DatabaseVersion sqlServer = worm::connection::parseDatabaseVersion("16.0.4215.2");
  if (postgres.major != 17 || postgres.minor != 4 || postgres.patch != 0 || sqlServer.major != 16 ||
      sqlServer.minor != 0 || sqlServer.patch != 4215 ||
      worm::connection::databasePermissionName(worm::connection::DatabasePermission::CreateTable) != "CREATE TABLE") {
    std::cerr << "Database diagnostic value conversion failed.\n";
    return 1;
  }

  DiagnosticClient connected;
  connected.ping();
  if (!connected.isConnected() || connected.statements != std::vector<std::string>{"SELECT 1", "SELECT 1"}) {
    std::cerr << "Client connectivity diagnostics did not perform a read-only round trip.\n";
    return 1;
  }

  DiagnosticClient disconnected{true};
  if (disconnected.isConnected() || disconnected.statements != std::vector<std::string>{"SELECT 1"}) {
    std::cerr << "Client connectivity diagnostics did not report a failed round trip.\n";
    return 1;
  }

  return 0;
}
