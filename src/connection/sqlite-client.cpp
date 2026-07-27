#include <connection/sqlite-client.hpp>
#include <errors/database-exception.hpp>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using worm::connection::SqliteClient;

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
  try {
    const char* message = sqlite3_errmsg(connection_);

    if (action == ErrorHandlingAction::CloseConnection) {
      sqlite3_close(connection_);
      throw worm::DatabaseException(message);
    } else if (action == ErrorHandlingAction::FinalizeStatement && statement) {
      sqlite3_finalize(statement);
      std::cerr << message << std::endl;
    }
  } catch (const std::exception& e) {
    sqlite3_close(connection_);
    throw worm::DatabaseException(e.what());
  }
}

SqliteClient& SqliteClient::getInstance(const worm::connection::ConnectionConfig& databaseConfig)
{
  static SqliteClient instance(databaseConfig.dbname.c_str());
  return instance;
}

worm::core::ResultSet SqliteClient::executeQuery(const std::string& query) const
{
  std::vector<worm::core::ResultRow> rows;
  sqlite3_stmt* statement = nullptr;
  int resultCode = sqlite3_prepare_v2(connection_, query.c_str(), static_cast<int>(query.size()), &statement, nullptr);

  if (resultCode != SQLITE_OK) {
    std::cerr << sqlite3_errmsg(connection_) << std::endl;

    if (statement)
      handleError(ErrorHandlingAction::FinalizeStatement, statement);

    return {};
  }

  if (isSelect(query)) {
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
  }

  if (resultCode != SQLITE_DONE)
    std::cerr << sqlite3_errmsg(connection_) << std::endl;

  sqlite3_finalize(statement);
  return worm::core::ResultSet{rows};
}
