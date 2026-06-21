#include <utils/client-factory.hpp>

#include <connection/sqlite-client.hpp>
#include <errors/database-exception.hpp>

#include <iostream>

int main()
{
  Json::Value config;
  config["dbname"] = ":memory:";

  worm::connection::Client& client = worm::getInstance(config, worm::DatabaseType::SQLite);
  if (dynamic_cast<worm::connection::SqliteClient*>(&client) == nullptr) {
    std::cerr << "Client factory returned the wrong SQLite client type.\n";
    return 1;
  }

  try {
    static_cast<void>(worm::getInstance(config, static_cast<worm::DatabaseType>(999)));
  } catch (const worm::DatabaseException&) {
    return 0;
  } catch (...) {
    std::cerr << "Client factory threw an unexpected error type.\n";
    return 1;
  }

  std::cerr << "Client factory accepted an unsupported database type.\n";
  return 1;
}
