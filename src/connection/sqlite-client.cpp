#include <connection/sqlite-client.hpp>
#include <errors/database-exception.hpp>

#include <iostream>
#include <string>

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

SqliteClient& SqliteClient::getInstance(const Json::Value& databaseConfig)
{
  const std::string databaseName = databaseConfig["dbname"].asString();
  static SqliteClient instance(databaseName.c_str());
  return instance;
}

Json::Value SqliteClient::executeQuery(const std::string& query) const
{
  Json::Value results;
  sqlite3_stmt* statement = nullptr;
  int resultCode = sqlite3_prepare_v2(connection_, query.c_str(), static_cast<int>(query.size()),
                                      &statement, nullptr);

  if (resultCode != SQLITE_OK) {
    std::cerr << sqlite3_errmsg(connection_) << std::endl;

    if (statement)
      handleError(ErrorHandlingAction::FinalizeStatement, statement);

    return Json::Value();
  }

  if (isSelect(query)) {
    int columnCount = sqlite3_column_count(statement);

    while ((resultCode = sqlite3_step(statement)) == SQLITE_ROW) {
      Json::Value result;

      for (int i = 0; i < columnCount; i++) {
        const char* columnName = sqlite3_column_name(statement, i);
        const auto* columnValue = reinterpret_cast<const char*>(sqlite3_column_text(statement, i));

        if (columnValue == nullptr)
          columnValue = "";

        result[columnName] = columnValue;
      }

      results["results"].append(result);
    }
  }

  if (resultCode != SQLITE_DONE)
    std::cerr << sqlite3_errmsg(connection_) << std::endl;

  sqlite3_finalize(statement);
  return results;
}
