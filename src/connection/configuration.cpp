#include <connection/configuration.hpp>

#include <connection/mysql-client.hpp>
#include <connection/pg-client.hpp>
#include <connection/sqlite-client.hpp>
#include <errors/database-exception.hpp>

namespace worm::connection
{
  Client& getInstance(const ConnectionConfig& connectionData, DatabaseType type)
  {
    switch (type) {
    case DatabaseType::PostgreSQL:
      return PgClient::getInstance(connectionData);
    case DatabaseType::MySQL:
      return MySqlClient::getInstance(connectionData);
    case DatabaseType::SQLite:
      return SqliteClient::getInstance(connectionData);
    default:
      throw DatabaseException("Unsupported database type.");
    }
  }
} // namespace worm::connection
