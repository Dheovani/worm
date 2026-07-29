#include <connection/sqlite-client.hpp>
#include <core/query/validator.hpp>
#include <errors/database-connection-exception.hpp>
#include <errors/query-execution-exception.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

using worm::connection::SqliteClient;

namespace
{
  int bindParameter(sqlite3_stmt* statement, int index, const worm::core::Parameter& parameter)
  {
    return std::visit(
      [statement, index](const auto& value) -> int
      {
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

SqliteClient::SqliteClient(const char* databaseName)
{
  int resultCode = sqlite3_open(databaseName, &connection_);

  // Error connecting to DB
  if (resultCode != SQLITE_OK)
    handleError(ErrorHandlingAction::CloseConnection);
}

SqliteClient::~SqliteClient()
{
  sqlite3_close(connection_);
}

void SqliteClient::handleError(ErrorHandlingAction action, sqlite3_stmt* statement) const
{
  const char* message = sqlite3_errmsg(connection_);

  if (action == ErrorHandlingAction::CloseConnection) {
    sqlite3_close(connection_);
    throw worm::DatabaseConnectionException(message);
  }

  if (action == ErrorHandlingAction::FinalizeStatement && statement) {
    sqlite3_finalize(statement);
    throw worm::QueryExecutionException(message);
  }
}

SqliteClient& SqliteClient::getInstance(const worm::connection::ConnectionConfig& databaseConfig)
{
  static SqliteClient instance(databaseConfig.dbname.c_str());
  return instance;
}

worm::core::ResultSet SqliteClient::executeQuery(const worm::core::Statement& statementData) const
{
  std::vector<worm::core::ResultRow> rows;
  sqlite3_stmt* statement = nullptr;
  int resultCode = sqlite3_prepare_v2(
    connection_,
    statementData.sql.c_str(),
    static_cast<int>(statementData.sql.size()),
    &statement,
    nullptr);

  if (resultCode != SQLITE_OK) {
    if (statement)
      handleError(ErrorHandlingAction::FinalizeStatement, statement);

    throw worm::QueryExecutionException(sqlite3_errmsg(connection_));
  }

  for (std::size_t i = 0; i < statementData.parameters.size(); i++) {
    resultCode = bindParameter(statement, static_cast<int>(i + 1), statementData.parameters[i]);

    if (resultCode != SQLITE_OK) {
      handleError(ErrorHandlingAction::FinalizeStatement, statement);
    }
  }

  if (core::isSelect(statementData.sql)) {
    int columnCount = sqlite3_column_count(statement);

    while ((resultCode = sqlite3_step(statement)) == SQLITE_ROW) {
      std::vector<worm::core::ResultColumn> columns;

      for (int i = 0; i < columnCount; i++) {
        const std::string columnName = sqlite3_column_name(statement, i);
        worm::core::Parameter columnValue = nullptr;

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
          columnValue = std::string{
            static_cast<const char*>(sqlite3_column_blob(statement, i)),
            static_cast<std::size_t>(sqlite3_column_bytes(statement, i))};
          break;
        }

        columns.push_back({columnName, columnValue});
      }

      rows.push_back({columns});
    }
  } else {
    resultCode = sqlite3_step(statement);
  }

  const std::uint64_t affectedRows = core::isSelect(statementData.sql)
    ? 0
    : static_cast<std::uint64_t>(sqlite3_changes(connection_));

  if (resultCode != SQLITE_DONE) {
    const std::string message = sqlite3_errmsg(connection_);
    sqlite3_finalize(statement);
    throw worm::QueryExecutionException(message);
  }

  sqlite3_finalize(statement);
  return worm::core::ResultSet{rows, affectedRows};
}
