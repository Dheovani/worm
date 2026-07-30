#include <connection/configuration.hpp>

#include <connection/sqlite-client.hpp>
#include <errors/unsupported-database-exception.hpp>

#include <iostream>
#include <memory>

int main()
{
  const worm::connection::ConnectionConfig config{
    .dbname = ":memory:",
  };

  std::unique_ptr<worm::connection::Client> client =
    worm::connection::makeClient(config, worm::connection::DatabaseType::SQLite);

  if (dynamic_cast<worm::connection::SqliteClient*>(client.get()) == nullptr ||
      client->type() != worm::connection::DatabaseType::SQLite) {
    std::cerr << "Connection configuration returned the wrong SQLite client type.\n";
    return 1;
  }

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
