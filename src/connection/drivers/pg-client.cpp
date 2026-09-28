#include <connection/drivers/pg-client.hpp>

#include <errors/database-connection-exception.hpp>
#include <errors/invalid-arg-exception.hpp>
#include <errors/migration-lock-exception.hpp>
#include <errors/query-execution-exception.hpp>
#include <errors/transaction-exception.hpp>
#include <utils/helpers.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>
#include <type_traits>
#include <variant>
#include <vector>

namespace
{
  constexpr pqxx::oid boolTypeOid = 16;
  constexpr pqxx::oid byteaTypeOid = 17;
  constexpr pqxx::oid int8TypeOid = 20;
  constexpr pqxx::oid int2TypeOid = 21;
  constexpr pqxx::oid int4TypeOid = 23;
  constexpr pqxx::oid float4TypeOid = 700;
  constexpr pqxx::oid float8TypeOid = 701;
  constexpr pqxx::oid numericTypeOid = 1700;
  constexpr std::byte emptyBinarySentinel{};

  std::string quoteConnectionValue(std::string_view value)
  {
    std::string quoted{"'"};
    quoted.reserve(value.size() + 2);

    for (const char character : value) {
      if (character == '\\' || character == '\'') {
        quoted += '\\';
      }

      quoted += character;
    }

    quoted += '\'';
    return quoted;
  }

  std::string pgConnectionData(const worm::connection::ConnectionConfig& databaseConfig)
  {
    using worm::utils::strings::replaceFirst;
    std::string conn = "host={host} port={port} dbname={dbname} user={username} password={password}";

    replaceFirst(conn, "{host}", quoteConnectionValue(databaseConfig.host));
    replaceFirst(conn, "{port}", quoteConnectionValue(databaseConfig.port));
    replaceFirst(conn, "{dbname}", quoteConnectionValue(databaseConfig.dbname));
    replaceFirst(conn, "{username}", quoteConnectionValue(databaseConfig.username));
    replaceFirst(conn, "{password}", quoteConnectionValue(databaseConfig.password));

    if (databaseConfig.timeoutConfig.connectionTimeout.has_value()) {
      const std::chrono::seconds timeout =
        worm::connection::timeoutSeconds(*databaseConfig.timeoutConfig.connectionTimeout);
      conn += " connect_timeout=" + std::to_string(timeout.count());
    }

    return conn;
  }

  pqxx::params pgParameters(const std::vector<worm::core::Parameter>& parameters)
  {
    pqxx::params values;
    values.reserve(parameters.size());

    for (const worm::core::Parameter& parameter : parameters) {
      std::visit(
        [&values](const auto& value) {
          using Value = std::decay_t<decltype(value)>;

          if constexpr (std::is_same_v<Value, std::nullptr_t>) {
            values.append();
          } else if constexpr (std::is_same_v<Value, worm::core::Decimal>) {
            values.append(value.value());
          } else if constexpr (std::is_same_v<Value, worm::core::Binary>) {
            const auto& binary = value.value();
            // libpq interprets a null data pointer as SQL NULL even when the binary length is zero.
            const std::byte* data = binary.empty() ? &emptyBinarySentinel : binary.data();
            values.append(pqxx::bytes_view{data, binary.size()});
          } else {
            values.append(value);
          }
        },
        parameter);
    }

    return values;
  }

  bool pgBoolean(const worm::core::ResultSet& result)
  {
    if (result.rowCount() != 1 || result.rows().front().columnCount() != 1) {
      return false;
    }

    const worm::core::Parameter& value = result.rows().front().columns.front().value;
    if (const auto* boolean = std::get_if<bool>(&value)) {
      return *boolean;
    }

    const auto* text = std::get_if<std::string>(&value);
    return text != nullptr && (*text == "t" || *text == "true" || *text == "1");
  }

  template <typename Field>
  worm::core::Parameter pgValue(const Field& field)
  {
    if (field.is_null()) {
      return nullptr;
    }

    switch (field.type()) {
    case boolTypeOid:
      return field.template as<bool>();
    case int2TypeOid:
    case int4TypeOid:
    case int8TypeOid:
      return field.template as<std::int64_t>();
    case float4TypeOid:
    case float8TypeOid:
      return field.template as<double>();
    case numericTypeOid:
      return worm::core::Decimal{field.view()};
    case byteaTypeOid:
      return worm::core::Binary{field.template as<pqxx::bytes>()};
    default:
      return std::string{field.view()};
    }
  }

  worm::connection::DatabasePermissionStatus pgPermissionStatus(const worm::core::Parameter& value)
  {
    const auto* granted = std::get_if<bool>(&value);
    return granted != nullptr && *granted ? worm::connection::DatabasePermissionStatus::Granted
                                          : worm::connection::DatabasePermissionStatus::Denied;
  }
} // namespace

namespace worm::connection
{
  PgClient::PgClient(const ConnectionConfig& databaseConfig)
    : Client(databaseConfig.cacheResults)
  {
    try {
      connection_ = std::make_unique<pqxx::connection>(pgConnectionData(databaseConfig));

      if (databaseConfig.timeoutConfig.queryTimeout.has_value()) {
        const std::chrono::milliseconds timeout = timeoutMilliseconds(*databaseConfig.timeoutConfig.queryTimeout);
        pqxx::work worker{*connection_};
        worker.exec("SET statement_timeout = " + std::to_string(timeout.count()));
        worker.commit();
      }
    } catch (const InvalidArgException&) {
      throw;
    } catch (const std::exception& error) {
      throw DatabaseConnectionException(error.what());
    }
  }

  worm::core::ResultSet PgClient::executeImpl(const worm::core::Statement& statement)
  {
    std::vector<worm::core::ResultRow> rows;
    pqxx::result response;

    try {
      if (innerTransaction_) {
        response = innerTransaction_->exec(statement.sql, pgParameters(statement.parameters));
      } else {
        pqxx::work worker = pqxx::work(*connection_);
        response = worker.exec(statement.sql, pgParameters(statement.parameters));
        worker.commit();
      }
    } catch (const std::exception& error) {
      throw QueryExecutionException(error.what());
    }

    for (pqxx::result::size_type i = 0; i < response.size(); i++) {
      std::vector<core::ResultColumn> columns;

      for (pqxx::result::size_type j = 0; j < response[i].size(); j++) {
        const auto field = response[i][j];
        const std::string columnName = field.name();
        columns.push_back({columnName, pgValue(field)});
      }

      rows.push_back({columns});
    }

    return core::ResultSet{rows, static_cast<std::uint64_t>(response.affected_rows())};
  }

  DatabaseType PgClient::type() const noexcept
  {
    return DatabaseType::PostgreSQL;
  }

  std::string PgClient::databaseVersionImpl()
  {
    const core::ResultSet result = executeImpl(core::Statement{"SHOW server_version"});
    if (result.rowCount() != 1 || result.rows().front().columns.empty()) {
      throw QueryExecutionException("PostgreSQL returned an invalid server version result.");
    }

    const auto* version = std::get_if<std::string>(&result.rows().front().columns.front().value);
    if (version == nullptr) {
      throw QueryExecutionException("PostgreSQL returned an incompatible server version value.");
    }

    return *version;
  }

  DatabasePermissions PgClient::databasePermissionsImpl()
  {
    const core::ResultSet result = executeImpl(
      core::Statement{
        "WITH worm_tables AS ("
        "SELECT format('%I.%I', schemaname, tablename) AS table_name, tableowner "
        "FROM pg_tables WHERE schemaname = current_schema()) "
        "SELECT "
        "COALESCE(bool_and(has_table_privilege(current_user, table_name, 'SELECT')), true), "
        "COALESCE(bool_and(has_table_privilege(current_user, table_name, 'INSERT')), true), "
        "COALESCE(bool_and(has_table_privilege(current_user, table_name, 'UPDATE')), true), "
        "COALESCE(bool_and(has_table_privilege(current_user, table_name, 'DELETE')), true), "
        "has_schema_privilege(current_user, current_schema(), 'CREATE'), "
        "COALESCE(bool_and(tableowner = current_user OR pg_has_role(current_user, tableowner, 'MEMBER')), true), "
        "COALESCE(bool_and(tableowner = current_user OR pg_has_role(current_user, tableowner, 'MEMBER')), true), "
        "COALESCE(bool_and(tableowner = current_user OR pg_has_role(current_user, tableowner, 'MEMBER')), true) "
        "FROM worm_tables"});

    if (result.rowCount() != 1 || result.rows().front().columnCount() != 8) {
      throw QueryExecutionException("PostgreSQL returned an invalid permission result.");
    }

    const core::ResultRow& row = result.rows().front();
    const DatabasePermission operations[]{DatabasePermission::Select,
      DatabasePermission::Insert,
      DatabasePermission::Update,
      DatabasePermission::Delete,
      DatabasePermission::CreateTable,
      DatabasePermission::AlterTable,
      DatabasePermission::DropTable,
      DatabasePermission::CreateIndex};
    DatabasePermissions permissions;
    permissions.reserve(row.columnCount());
    for (std::size_t index = 0; index < row.columnCount(); ++index) {
      permissions.push_back(
        {operations[index], pgPermissionStatus(row.columns[index].value), "Current PostgreSQL schema"});
    }
    return permissions;
  }

  void PgClient::beginTransactionImpl()
  {
    if (innerTransaction_) {
      throw worm::TransactionException("A PostgreSQL transaction is already active.");
    }

    innerTransaction_ = std::make_unique<pqxx::work>(*connection_);
  }

  void PgClient::commitTransactionImpl()
  {
    if (!innerTransaction_) {
      throw worm::TransactionException("There is no active PostgreSQL transaction to commit.");
    }

    try {
      innerTransaction_->commit();
      innerTransaction_.reset();
    } catch (const std::exception& error) {
      throw worm::QueryExecutionException(error.what());
    }
  }

  void PgClient::rollbackTransactionImpl()
  {
    if (!innerTransaction_) {
      throw worm::TransactionException("There is no active PostgreSQL transaction to rollback.");
    }

    try {
      innerTransaction_->abort();
      innerTransaction_.reset();
    } catch (const std::exception& error) {
      throw worm::QueryExecutionException(error.what());
    }
  }

  void PgClient::acquireMigrationLockImpl(std::string_view name, std::chrono::milliseconds timeout)
  {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    const core::Statement statement{
      "SELECT pg_try_advisory_lock(hashtextextended($1, 0)) AS acquired",
      {std::string{name}},
    };

    do {
      if (pgBoolean(executeImpl(statement))) {
        return;
      }

      if (std::chrono::steady_clock::now() >= deadline) {
        break;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds{25});
    } while (true);

    throw MigrationLockException("Timed out while acquiring PostgreSQL migration lock '{}'.", name);
  }

  void PgClient::releaseMigrationLockImpl(std::string_view name, bool completed)
  {
    static_cast<void>(completed);
    const core::ResultSet result = executeImpl(
      {
        "SELECT pg_advisory_unlock(hashtextextended($1, 0)) AS released",
        {std::string{name}},
      });

    if (!pgBoolean(result)) {
      throw MigrationLockException("PostgreSQL connection does not own migration lock '{}'.", name);
    }
  }
} // namespace worm::connection
