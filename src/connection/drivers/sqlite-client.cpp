#include <connection/drivers/sqlite-client.hpp>
#include <errors/database-connection-exception.hpp>
#include <errors/query-execution-exception.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace
{
  int bindParameter(sqlite3_stmt* statement, int index, const worm::core::Parameter& parameter)
  {
    return std::visit(
      [statement, index](const auto& value) -> int {
        using Value = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<Value, std::nullptr_t>) {
          return sqlite3_bind_null(statement, index);
        } else if constexpr (std::is_same_v<Value, std::int64_t>) {
          return sqlite3_bind_int64(statement, index, value);
        } else if constexpr (std::is_same_v<Value, double>) {
          return sqlite3_bind_double(statement, index, value);
        } else if constexpr (std::is_same_v<Value, bool>) {
          return sqlite3_bind_int64(statement, index, value ? 1 : 0);
        } else {
          return sqlite3_bind_text(statement, index, value.c_str(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
        }
      },
      parameter);
  }
} // namespace

namespace worm::connection
{
  void SqliteClient::ConnectionDeleter::operator()(sqlite3* connection) const noexcept
  {
    if (connection != nullptr) {
      sqlite3_close(connection);
    }
  }

  SqliteClient::SqliteClient(const ConnectionConfig& databaseConfig)
    : Client(databaseConfig.cacheResults),
      connection_(nullptr)
  {
    sqlite3* connection = nullptr;
    const int resultCode = sqlite3_open(databaseConfig.dbname.c_str(), &connection);
    connection_.reset(connection);

    if (resultCode != SQLITE_OK) {
      throwConnectionError();
    }
  }

  void SqliteClient::throwConnectionError()
  {
    const std::string message = sqlite3_errmsg(connection_.get());
    connection_.reset();
    throw DatabaseConnectionException(message);
  }

  void SqliteClient::throwStatementError(sqlite3_stmt* statement) const
  {
    const std::string message = sqlite3_errmsg(connection_.get());
    sqlite3_finalize(statement);
    throw QueryExecutionException(message);
  }

  void SqliteClient::executeTransactionCommand(const char* sql)
  {
    char* rawError = nullptr;
    const int resultCode = sqlite3_exec(connection_.get(), sql, nullptr, nullptr, &rawError);

    if (resultCode == SQLITE_OK) {
      return;
    }

    const std::string message = rawError != nullptr ? rawError : sqlite3_errmsg(connection_.get());
    sqlite3_free(rawError);

    throw QueryExecutionException(message);
  }

  worm::core::ResultSet SqliteClient::executeImpl(const worm::core::Statement& statementData)
  {
    std::vector<core::ResultRow> rows;
    sqlite3_stmt* statement = nullptr;
    int resultCode = sqlite3_prepare_v2(
      connection_.get(), statementData.sql.c_str(), static_cast<int>(statementData.sql.size()), &statement, nullptr);

    if (resultCode != SQLITE_OK) {
      if (statement != nullptr) {
        throwStatementError(statement);
      }

      throw QueryExecutionException(sqlite3_errmsg(connection_.get()));
    }

    const bool readOnly = sqlite3_stmt_readonly(statement) != 0;

    for (std::size_t i = 0; i < statementData.parameters.size(); i++) {
      resultCode = bindParameter(statement, static_cast<int>(i + 1), statementData.parameters[i]);

      if (resultCode != SQLITE_OK) {
        throwStatementError(statement);
      }
    }

    const int columnCount = sqlite3_column_count(statement);
    while ((resultCode = sqlite3_step(statement)) == SQLITE_ROW) {
      std::vector<core::ResultColumn> columns;

      for (int i = 0; i < columnCount; i++) {
        const std::string columnName = sqlite3_column_name(statement, i);
        core::Parameter columnValue = nullptr;

        switch (sqlite3_column_type(statement, i)) {
        case SQLITE_INTEGER:
          columnValue = static_cast<std::int64_t>(sqlite3_column_int64(statement, i));
          break;
        case SQLITE_FLOAT:
          columnValue = sqlite3_column_double(statement, i);
          break;
        case SQLITE_TEXT:
          columnValue = reinterpret_cast<const char*>(sqlite3_column_text(statement, i));
          break;
        case SQLITE_NULL:
          columnValue = nullptr;
          break;
        default:
          columnValue = std::string{static_cast<const char*>(sqlite3_column_blob(statement, i)),
            static_cast<std::size_t>(sqlite3_column_bytes(statement, i))};
          break;
        }

        columns.push_back({columnName, columnValue});
      }

      rows.push_back({columns});
    }

    const std::uint64_t affectedRows = readOnly ? 0 : static_cast<std::uint64_t>(sqlite3_changes(connection_.get()));

    if (resultCode != SQLITE_DONE) {
      throwStatementError(statement);
    }

    sqlite3_finalize(statement);
    return core::ResultSet{rows, affectedRows};
  }

  DatabaseType SqliteClient::type() const noexcept
  {
    return DatabaseType::SQLite;
  }

  void SqliteClient::beginTransactionImpl()
  {
    executeTransactionCommand("BEGIN TRANSACTION");
  }

  void SqliteClient::commitTransactionImpl()
  {
    executeTransactionCommand("COMMIT");
  }

  void SqliteClient::rollbackTransactionImpl()
  {
    executeTransactionCommand("ROLLBACK");
  }
} // namespace worm::connection
