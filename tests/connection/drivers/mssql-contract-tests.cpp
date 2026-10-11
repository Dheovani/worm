#include "driver-contract.hpp"
#include "mssql-test-support.hpp"

#include <connection/drivers/mssql-client.hpp>
#include <connection/schema-inspector.hpp>
#include <core/query/sql-builder.hpp>
#include <errors/database-connection-exception.hpp>

#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
  constexpr int skippedTest = 77;

  void resetSchema(const worm::connection::ConnectionConfig& config)
  {
    const worm::tests::mssql::OdbcConnection connection{config};
    for (const char* table : {"worm_schema_child",
           "worm_schema_contract",
           "worm_driver_contract",
           "worm_generated_key_contract",
           "worm_migration_contract",
           "worm_migration_failure",
           "_worm_migrations"}) {
      connection.execute(
        "IF OBJECT_ID(N'dbo." + std::string{table} + "', N'U') IS NOT NULL DROP TABLE [dbo].[" + table + "]");
    }
    connection.execute(
      "CREATE TABLE [dbo].[worm_generated_key_contract] ([id] BIGINT IDENTITY(1,1) PRIMARY KEY, "
      "[label] NVARCHAR(255) NOT NULL)");
    connection.execute(
      "CREATE TABLE [dbo].[worm_driver_contract] ([id] VARCHAR(64) PRIMARY KEY, [label] VARCHAR(255) NOT NULL, "
      "[note] VARCHAR(255) NULL, [amount] DECIMAL(30,6) NOT NULL, [payload] VARBINARY(MAX) NOT NULL)");
    connection.execute(
      "CREATE TABLE [dbo].[worm_schema_contract] ([id] BIGINT IDENTITY(1,1) PRIMARY KEY, "
      "[email] NVARCHAR(255) UNIQUE, [tenant] NVARCHAR(64), [external_id] NVARCHAR(64), "
      "[created_at] DATETIMEOFFSET NOT NULL DEFAULT SYSDATETIMEOFFSET(), "
      "CONSTRAINT [uq_worm_schema_tenant_external] UNIQUE ([tenant], [external_id]))");
    connection.execute(
      "CREATE TABLE [dbo].[worm_schema_child] ([id] BIGINT PRIMARY KEY, [parent_id] BIGINT NOT NULL, "
      "CONSTRAINT [fk_worm_schema_parent] FOREIGN KEY ([parent_id]) REFERENCES "
      "[dbo].[worm_schema_contract]([id]) ON DELETE CASCADE)");
    connection.execute(
      "CREATE UNIQUE INDEX [ix_worm_schema_child_composite] ON [dbo].[worm_schema_child] ([parent_id], [id])");
    connection.execute("CREATE INDEX [ix_worm_schema_child_parent] ON [dbo].[worm_schema_child] ([parent_id])");
  }
} // namespace

int main()
try {
  const std::string databaseName = worm::tests::mssql::environmentValue("WORM_TEST_MSSQL_DBNAME");
  if (databaseName.empty()) {
    return skippedTest;
  }

  const worm::connection::ConnectionConfig config = worm::tests::mssql::connectionConfig();

  const auto client = std::make_shared<worm::connection::MsSqlClient>(config);
  resetSchema(config);
  const worm::core::SqlServerBuilder sqlBuilder;

  worm::tests::runDriverContract(client, sqlBuilder, worm::connection::DatabaseType::MSSQL);
  worm::tests::runMigrationLockContract(
    client,
    std::make_shared<worm::connection::MsSqlClient>(config),
    sqlBuilder,
    "dbo");
  worm::tests::runMigrationExecutionContract(client, sqlBuilder, "dbo");

  const worm::connection::SchemaInspector inspector{*client};
  const auto schema = inspector.inspect();
  const auto* table = schema.findTable("dbo", "worm_schema_contract");
  const auto* child = schema.findTable("dbo", "worm_schema_child");
  if (table == nullptr || table->columns.size() != 5 || table->primaryKey != std::vector<std::string>{"id"} ||
      !table->columns[0].generated || !table->columns[1].unique || table->columns[2].unique ||
      table->columns[4].type.kind != worm::core::ColumnTypeKind::DateTime || !table->columns[4].type.withTimeZone ||
      child == nullptr || child->primaryKey != std::vector<std::string>{"id"} ||
      child->foreignKey != std::vector<std::string>{"parent_id -> dbo.worm_schema_contract.id"} ||
      child->indexes != std::vector<std::string>{"ix_worm_schema_child_composite (parent_id, id) UNIQUE",
                          "ix_worm_schema_child_parent (parent_id)"}) {
    throw std::runtime_error("SQL Server schema introspection did not return the contract table.");
  }

  auto invalid = config;
  invalid.password = "Worm-invalid-password@12345";
  bool connectionErrorNormalized = false;
  try {
    static_cast<void>(worm::connection::MsSqlClient{invalid});
  } catch (const worm::DatabaseConnectionException& error) {
    connectionErrorNormalized = std::string{error.what()}.find(invalid.password) == std::string::npos;
  }
  worm::tests::requireContract(
    connectionErrorNormalized,
    "SQL Server did not normalize or redact an authentication failure.");

  return 0;
} catch (const std::exception& error) {
  std::cerr << "MSSQL driver contract failed: " << error.what() << '\n';
  return 1;
}
