#include <connection/client.hpp>

#include <iostream>
#include <string>

namespace
{
  class TestClient final : public worm::connection::Client
  {
  public:
    worm::core::ResultSet execute(const worm::core::Statement&) override
    {
      return {};
    }

    worm::connection::DatabaseType type() const noexcept override
    {
      return worm::connection::DatabaseType::SQLite;
    }
  };
} // namespace

int main()
{
  TestClient client;
  static_cast<void>(client);

  if (worm::connection::databaseTypes.at("postgresql") != worm::connection::DatabaseType::PostgreSQL ||
      worm::connection::databaseTypes.at("mysql") != worm::connection::DatabaseType::MySQL ||
      worm::connection::databaseTypes.at("sqlite") != worm::connection::DatabaseType::SQLite) {
    std::cerr << "Database type mapping is incorrect.\n";
    return 1;
  }

  return 0;
}
