#include <connection/configuration.hpp>

#if defined(WORM_HAS_SQLITE_DRIVER)
#include <connection/drivers/sqlite-client.hpp>
#endif

#include <errors/unsupported-database-exception.hpp>

#include <iostream>
#include <memory>

int main()
{
  const worm::connection::ConnectionConfig config{
    .dbname = ":memory:",
  };

#if defined(WORM_HAS_SQLITE_DRIVER)
  std::unique_ptr<worm::connection::Client> client =
    worm::connection::makeClient(config, worm::connection::DatabaseType::SQLite);

  if (dynamic_cast<worm::connection::SqliteClient*>(client.get()) == nullptr ||
      client->type() != worm::connection::DatabaseType::SQLite) {
    std::cerr << "Connection configuration returned the wrong SQLite client type.\n";
    return 1;
  }
#else
  try {
    static_cast<void>(worm::connection::makeClient(config, worm::connection::DatabaseType::SQLite));
    std::cerr << "Connection configuration created a disabled SQLite driver.\n";
    return 1;
  } catch (const worm::UnsupportedDatabaseException&) {}
#endif

#if !defined(WORM_HAS_POSTGRESQL_DRIVER)
  try {
    static_cast<void>(worm::connection::makeClient(config, worm::connection::DatabaseType::PostgreSQL));
    std::cerr << "Connection configuration created a disabled PostgreSQL driver.\n";
    return 1;
  } catch (const worm::UnsupportedDatabaseException&) {}
#endif

#if !defined(WORM_HAS_MYSQL_DRIVER)
  try {
    static_cast<void>(worm::connection::makeClient(config, worm::connection::DatabaseType::MySQL));
    std::cerr << "Connection configuration created a disabled MySQL driver.\n";
    return 1;
  } catch (const worm::UnsupportedDatabaseException&) {}
#endif

#if !defined(WORM_HAS_MSSQL_DRIVER)
  try {
    static_cast<void>(worm::connection::makeClient(config, worm::connection::DatabaseType::MSSQL));
    std::cerr << "Connection configuration created a disabled MSSQL driver.\n";
    return 1;
  } catch (const worm::UnsupportedDatabaseException&) {}
#endif

  try {
    static_cast<void>(worm::connection::makeClient(config, static_cast<worm::connection::DatabaseType>(999)));
  } catch (const worm::UnsupportedDatabaseException&) {
    return 0;
  } catch (...) {
    std::cerr << "Connection configuration threw an unexpected error type.\n";
    return 1;
  }

  std::cerr << "Connection configuration accepted an unsupported database type.\n";
  return 1;
}
