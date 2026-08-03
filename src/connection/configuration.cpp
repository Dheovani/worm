#include <connection/configuration.hpp>

#include <errors/unsupported-database-exception.hpp>

#if defined(WORM_HAS_MYSQL_DRIVER)
#include <connection/drivers/mysql-client.hpp>
#endif

#if defined(WORM_HAS_POSTGRESQL_DRIVER)
#include <connection/drivers/pg-client.hpp>
#endif

#if defined(WORM_HAS_SQLITE_DRIVER)
#include <connection/drivers/sqlite-client.hpp>
#endif

namespace worm::connection
{
  std::unique_ptr<Client> makeClient(const ConnectionConfig& connectionData, DatabaseType type)
  {
    switch (type) {
    case DatabaseType::PostgreSQL:
#if defined(WORM_HAS_POSTGRESQL_DRIVER)
      return std::make_unique<PgClient>(connectionData);
#else
      throw UnsupportedDatabaseException("PostgreSQL driver is not enabled in this build.");
#endif
    case DatabaseType::MySQL:
#if defined(WORM_HAS_MYSQL_DRIVER)
      return std::make_unique<MySqlClient>(connectionData);
#else
      throw UnsupportedDatabaseException("MySQL driver is not enabled in this build.");
#endif
    case DatabaseType::SQLite:
#if defined(WORM_HAS_SQLITE_DRIVER)
      return std::make_unique<SqliteClient>(connectionData);
#else
      throw UnsupportedDatabaseException("SQLite driver is not enabled in this build.");
#endif
    default:
      throw UnsupportedDatabaseException("Unsupported database type.");
    }
  }
} // namespace worm::connection
