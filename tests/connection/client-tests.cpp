#include <connection/client.hpp>

#include <iostream>
#include <string>

namespace
{
  class TestClient final : public worm::connection::Client
  {
  public:
    worm::connection::DatabaseType type() const noexcept override
    {
      return worm::connection::DatabaseType::SQLite;
    }

  private:
    worm::core::ResultSet executeImpl(const worm::core::Statement&) override
    {
      return {};
    }

    void beginTransactionImpl() override {}

    void rollbackTransactionImpl() override {}

    void commitTransactionImpl() override {}
  };
} // namespace

int main()
{
  TestClient client;
  static_cast<void>(client);

  if (worm::connection::databaseTypes.at("postgresql") != worm::connection::DatabaseType::PostgreSQL ||
      worm::connection::databaseTypes.at("mysql") != worm::connection::DatabaseType::MySQL ||
      worm::connection::databaseTypes.at("sqlite") != worm::connection::DatabaseType::SQLite ||
      worm::connection::databaseTypes.at("mssql") != worm::connection::DatabaseType::MSSQL) {
    std::cerr << "Database type mapping is incorrect.\n";
    return 1;
  }

  return 0;
}
