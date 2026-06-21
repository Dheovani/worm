#include <utils/client-factory.hpp>

#include <connection/mysql-client.hpp>
#include <connection/pg-client.hpp>
#include <connection/sqlite-client.hpp>
#include <errors/database-exception.hpp>

worm::connection::Client& worm::getInstance(const Json::Value& connectionData, DatabaseType type)
{
  switch (type) {
  case DatabaseType::PostgreSQL:
    return connection::PgClient::getInstance(connectionData);
  case DatabaseType::MySQL:
    return connection::MySqlClient::getInstance(connectionData);
  case DatabaseType::SQLite:
    return connection::SqliteClient::getInstance(connectionData);
  default:
    throw DatabaseException("Unsupported database type.");
  }
}
