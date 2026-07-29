#include <connection/client.hpp>

#include <iostream>
#include <string>

namespace
{
  class TestClient final : public worm::connection::Client
  {
  public:
    worm::core::ResultSet executeQuery(const worm::core::Statement&) const override
    {
      return {};
    }
  };
} // namespace

int main()
{
  const TestClient client;
  static_cast<void>(client);

  if (worm::connection::databaseTypes.at("postgresql") != worm::connection::DatabaseType::PostgreSQL ||
      worm::connection::databaseTypes.at("mysql") != worm::connection::DatabaseType::MySQL ||
      worm::connection::databaseTypes.at("sqlite") != worm::connection::DatabaseType::SQLite) {
    std::cerr << "Database type mapping is incorrect.\n";
    return 1;
  }

  return 0;
}
