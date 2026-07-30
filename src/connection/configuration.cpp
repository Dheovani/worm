#include <connection/configuration.hpp>

#include <connection/mysql-client.hpp>
#include <connection/pg-client.hpp>
#include <connection/sqlite-client.hpp>
#include <errors/unsupported-database-exception.hpp>

namespace worm::connection
{
  std::unique_ptr<Client> makeClient(const ConnectionConfig& connectionData, DatabaseType type)
  {
    switch (type) {
    case DatabaseType::PostgreSQL:
      return std::make_unique<PgClient>(connectionData);
    case DatabaseType::MySQL:
      return std::make_unique<MySqlClient>(connectionData);
    case DatabaseType::SQLite:
      return std::make_unique<SqliteClient>(connectionData);
    default:
      throw UnsupportedDatabaseException("Unsupported database type.");
    }
  }
} // namespace worm::connection
