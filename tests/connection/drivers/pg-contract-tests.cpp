#include "driver-contract.hpp"

#include <connection/configuration.hpp>
#include <connection/drivers/pg-client.hpp>
#include <core/query/sql-builder.hpp>
#include <pqxx/pqxx>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
  constexpr int skippedTest = 77;

  std::string environmentValue(const char* name, const char* fallback = nullptr)
  {
    if (const char* value = std::getenv(name)) {
      return value;
    }

    return fallback != nullptr ? fallback : "";
  }

  std::string connectionString(const worm::connection::ConnectionConfig& config)
  {
    return "host=" + config.host + " port=" + config.port + " dbname=" + config.dbname + " user=" + config.username +
           " password=" + config.password;
  }

  void resetSchema(const worm::connection::ConnectionConfig& config)
  {
    pqxx::connection connection{connectionString(config)};
    pqxx::work transaction{connection};
    transaction.exec("DROP TABLE IF EXISTS worm_driver_contract");
    transaction.exec("CREATE TABLE worm_driver_contract ("
                     "id TEXT PRIMARY KEY, label TEXT NOT NULL, note TEXT NULL)");
    transaction.commit();
  }
} // namespace

int main()
try {
  const std::string databaseName = environmentValue("WORM_TEST_POSTGRES_DBNAME");
  if (databaseName.empty()) {
    return skippedTest;
  }

  const worm::connection::ConnectionConfig config{
    .host = environmentValue("WORM_TEST_POSTGRES_HOST", "127.0.0.1"),
    .username = environmentValue("WORM_TEST_POSTGRES_USERNAME", "worm"),
    .password = environmentValue("WORM_TEST_POSTGRES_PASSWORD", "worm"),
    .dbname = databaseName,
    .port = environmentValue("WORM_TEST_POSTGRES_PORT", "5432"),
  };

  resetSchema(config);

  const auto client = std::make_shared<worm::connection::PgClient>(config);
  const worm::core::PgBuilder sqlBuilder;

  worm::tests::runDriverContract(client, sqlBuilder, worm::connection::DatabaseType::PostgreSQL);

  return 0;
} catch (const std::exception& error) {
  std::cerr << "PostgreSQL driver contract failed: " << error.what() << "\n";
  return 1;
}
