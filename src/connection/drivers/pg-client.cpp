#include <connection/drivers/pg-client.hpp>

#include <errors/database-connection-exception.hpp>
#include <errors/query-execution-exception.hpp>
#include <errors/transaction-exception.hpp>
#include <utils/helpers.hpp>

#include <cstdint>
#include <type_traits>
#include <variant>
#include <vector>

namespace
{
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
          } else {
            values.append(value);
          }
        },
        parameter);
    }

    return values;
  }
} // namespace

namespace worm::connection
{
  PgClient::PgClient(const ConnectionConfig& databaseConfig)
  try
    : Client(databaseConfig.cacheResults),
      connection_(std::make_unique<pqxx::connection>(pgConnectionData(databaseConfig))),
      innerTransaction_(nullptr) {
  } catch (const std::exception& error) {
    throw DatabaseConnectionException(error.what());
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
        const std::string columnName = response[i][j].name();
        core::Parameter columnValue = nullptr;

        if (!response[i][j].is_null()) {
          columnValue = response[i][j].c_str();
        }

        columns.push_back({columnName, columnValue});
      }

      rows.push_back({columns});
    }

    return core::ResultSet{rows, static_cast<std::uint64_t>(response.affected_rows())};
  }

  DatabaseType PgClient::type() const noexcept
  {
    return DatabaseType::PostgreSQL;
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
} // namespace worm::connection
