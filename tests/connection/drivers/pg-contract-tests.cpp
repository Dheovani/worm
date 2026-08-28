#include "driver-contract.hpp"

#include <connection/configuration.hpp>
#include <connection/drivers/pg-client.hpp>
#include <connection/schema-inspector.hpp>
#include <core/query/sql-builder.hpp>
#include <pqxx/pqxx>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

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
    transaction.exec("DROP TABLE IF EXISTS worm_schema_contract");
    transaction.exec("DROP TYPE IF EXISTS worm_contract_status");
    transaction.exec("CREATE TYPE worm_contract_status AS ENUM ('active','on''hold')");
    transaction.exec(
      "CREATE TABLE worm_driver_contract ("
      "id TEXT PRIMARY KEY, label TEXT NOT NULL, note TEXT NULL, amount NUMERIC(30,6) NOT NULL, "
      "payload BYTEA NOT NULL)");
    transaction.exec(
      "CREATE TABLE worm_schema_contract ("
      "id TEXT PRIMARY KEY, email TEXT UNIQUE, tenant TEXT, external_id TEXT, "
      "status worm_contract_status NOT NULL, "
      "UNIQUE (tenant, external_id))");
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

  const worm::connection::SchemaInspector inspector{*client};
  const auto schema = inspector.inspect();
  const auto* table = schema.findTable("public", "worm_schema_contract");
  if (table == nullptr || table->columns.size() != 5 || table->primaryKey != std::vector<std::string>{"id"} ||
      !table->columns[1].unique || table->columns[2].unique || table->columns[3].unique ||
      table->columns[4].type.kind != worm::core::ColumnTypeKind::Enum ||
      table->columns[4].type.enumeration->schema != "public" ||
      table->columns[4].type.enumeration->name != "worm_contract_status" ||
      table->columns[4].type.enumeration->values != std::vector<std::string>{"active", "on'hold"}) {
    throw std::runtime_error("PostgreSQL schema introspection did not return the contract table.");
  }

  return 0;
} catch (const std::exception& error) {
  std::cerr << "PostgreSQL driver contract failed: " << error.what() << "\n";
  return 1;
}
