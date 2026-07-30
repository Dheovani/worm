#include <connection/pg-client.hpp>

#include <errors/database-connection-exception.hpp>
#include <errors/query-execution-exception.hpp>
#include <errors/transaction-exception.hpp>

#include <cstdint>
#include <type_traits>
#include <variant>
#include <vector>

namespace
{
  std::string pgConnectionData(const worm::connection::ConnectionConfig& databaseConfig)
  {
    return
      " host=" + databaseConfig.host +
      " port=" + databaseConfig.port +
      " dbname=" + databaseConfig.dbname +
      " user=" + databaseConfig.username +
      " password=" + databaseConfig.password;
  }

  pqxx::params pgParameters(const std::vector<worm::core::Parameter>& parameters)
  {
    pqxx::params values;
    values.reserve(parameters.size());

    for (const worm::core::Parameter& parameter : parameters) {
      std::visit(
        [&values](const auto& value)
        {
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
    : connection_(std::make_unique<pqxx::connection>(pgConnectionData(databaseConfig))),
      innerTransaction_(nullptr)
  {}
  catch (const std::exception& error) {
    throw DatabaseConnectionException(error.what());
  }

  worm::core::ResultSet PgClient::execute(const worm::core::Statement& statement)
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

  void PgClient::commitTransaction()
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

  void PgClient::rollbackTransaction()
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
