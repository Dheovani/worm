#include "driver-contract.hpp"

#include <connection/configuration.hpp>
#include <connection/drivers/mysql-client.hpp>
#include <core/query/sql-builder.hpp>
#include <mysql/mysql.h>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
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

  void executeSql(MYSQL* connection, const char* sql)
  {
    if (mysql_query(connection, sql)) {
      throw std::runtime_error(mysql_error(connection));
    }
  }

  void resetSchema(const worm::connection::ConnectionConfig& config)
  {
    std::unique_ptr<MYSQL, decltype(&mysql_close)> connection{mysql_init(nullptr), mysql_close};
    if (!connection) {
      throw std::runtime_error("Could not initialize the MySQL contract connection.");
    }

    const unsigned int port = static_cast<unsigned int>(std::stoul(config.port));
    if (mysql_real_connect(
          connection.get(),
          config.host.c_str(),
          config.username.c_str(),
          config.password.c_str(),
          config.dbname.c_str(),
          port,
          nullptr,
          0) == nullptr) {
      throw std::runtime_error(mysql_error(connection.get()));
    }

    executeSql(connection.get(), "DROP TABLE IF EXISTS worm_driver_contract");
    executeSql(
      connection.get(),
      "CREATE TABLE worm_driver_contract ("
      "id VARCHAR(64) PRIMARY KEY, label VARCHAR(255) NOT NULL, note VARCHAR(255) NULL)");
  }
} // namespace

int main()
try
{
  const std::string databaseName = environmentValue("WORM_TEST_MYSQL_DBNAME");
  if (databaseName.empty()) {
    return skippedTest;
  }

  const worm::connection::ConnectionConfig config{
    .host = environmentValue("WORM_TEST_MYSQL_HOST", "127.0.0.1"),
    .username = environmentValue("WORM_TEST_MYSQL_USERNAME", "worm"),
    .password = environmentValue("WORM_TEST_MYSQL_PASSWORD", "worm"),
    .dbname = databaseName,
    .port = environmentValue("WORM_TEST_MYSQL_PORT", "3306"),
  };

  resetSchema(config);

  worm::connection::MySqlClient client{config};
  const worm::core::MySqlBuilder sqlBuilder;

  worm::tests::runDriverContract(
    client,
    sqlBuilder,
    worm::connection::DatabaseType::MySQL);

  return 0;
} catch (const std::exception& error) {
  std::cerr << "MySQL driver contract failed: " << error.what() << "\n";
  return 1;
}
