#include <connection/configuration.hpp>

#include <errors/invalid-arg-exception.hpp>
#include <errors/unsupported-database-exception.hpp>
#include <utils/logger.hpp>

#if defined(WORM_HAS_MYSQL_DRIVER)
#include <connection/drivers/mysql-client.hpp>
#endif

#if defined(WORM_HAS_POSTGRESQL_DRIVER)
#include <connection/drivers/pg-client.hpp>
#endif

#if defined(WORM_HAS_SQLITE_DRIVER)
#include <connection/drivers/sqlite-client.hpp>
#endif

#if defined(WORM_HAS_MSSQL_DRIVER)
#include <connection/drivers/mssql-client.hpp>
#endif

namespace worm::connection
{
  bool isDriverEnabled(DatabaseType type) noexcept
  {
    switch (type) {
    case DatabaseType::PostgreSQL:
#if defined(WORM_HAS_POSTGRESQL_DRIVER)
      return true;
#else
      return false;
#endif
    case DatabaseType::MySQL:
#if defined(WORM_HAS_MYSQL_DRIVER)
      return true;
#else
      return false;
#endif
    case DatabaseType::SQLite:
#if defined(WORM_HAS_SQLITE_DRIVER)
      return true;
#else
      return false;
#endif
    case DatabaseType::MSSQL:
#if defined(WORM_HAS_MSSQL_DRIVER)
      return true;
#else
      return false;
#endif
    }

    return false;
  }

  std::string_view databaseTypeName(DatabaseType type) noexcept
  {
    switch (type) {
    case DatabaseType::PostgreSQL:
      return "PostgreSQL";
    case DatabaseType::MySQL:
      return "MySQL";
    case DatabaseType::SQLite:
      return "SQLite";
    case DatabaseType::MSSQL:
      return "Microsoft SQL Server";
    }

    return "Unknown";
  }

  std::chrono::milliseconds timeoutMilliseconds(std::chrono::milliseconds timeout)
  {
    if (timeout.count() < 0) {
      throw InvalidArgException("Timeout values cannot be negative.");
    }

    return timeout;
  }

  std::chrono::seconds timeoutSeconds(std::chrono::milliseconds timeout)
  {
    timeout = timeoutMilliseconds(timeout);

    if (timeout.count() == 0) {
      return std::chrono::seconds{0};
    }

    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(timeout);
    if (seconds < timeout) {
      return seconds + std::chrono::seconds{1};
    }

    return seconds;
  }

  std::unique_ptr<Client> makeClient(const ConnectionConfig& connectionData, DatabaseType type)
  {
    logger.log(
      LogLevel::Info,
      "Opening database connection.",
      {
        {"driver", std::string{databaseTypeName(type)}},
        {"host", connectionData.host},
        {"port", connectionData.port},
        {"database", connectionData.dbname},
      });

    std::unique_ptr<Client> client;
    switch (type) {
    case DatabaseType::PostgreSQL:
#if defined(WORM_HAS_POSTGRESQL_DRIVER)
      client = std::make_unique<PgClient>(connectionData);
      break;
#else
      throw UnsupportedDatabaseException("PostgreSQL driver is not enabled in this build.");
#endif
    case DatabaseType::MySQL:
#if defined(WORM_HAS_MYSQL_DRIVER)
      client = std::make_unique<MySqlClient>(connectionData);
      break;
#else
      throw UnsupportedDatabaseException("MySQL driver is not enabled in this build.");
#endif
    case DatabaseType::SQLite:
#if defined(WORM_HAS_SQLITE_DRIVER)
      client = std::make_unique<SqliteClient>(connectionData);
      break;
#else
      throw UnsupportedDatabaseException("SQLite driver is not enabled in this build.");
#endif
    case DatabaseType::MSSQL:
#if defined(WORM_HAS_MSSQL_DRIVER)
      client = std::make_unique<MsSqlClient>(connectionData);
      break;
#else
      throw UnsupportedDatabaseException("MSSQL driver is not enabled in this build.");
#endif
    default:
      throw UnsupportedDatabaseException("Unsupported database type.");
    }

    logger.log(
      LogLevel::Debug,
      "Database connection opened.",
      {
        {"driver", std::string{databaseTypeName(type)}},
      });
    return client;
  }
} // namespace worm::connection
