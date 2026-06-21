#include <connection/client.hpp>

#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace
{
  class TestClient final : public worm::connection::Client
  {
  public:
    bool isSelectQuery(const std::string& query) const
    {
      return isSelect(query);
    }

    Json::Value executeQuery(const std::string&) const override
    {
      return {};
    }
  };
} // namespace

int main()
{
  const TestClient client;
  const std::vector<std::pair<std::string, bool>> cases = {
      {"SELECT * FROM users", true},
      {"select id from users", true},
      {"  SeLeCt 1", true},
      {"INSERT INTO users VALUES (1)", false},
      {"WITH users AS (SELECT 1) SELECT * FROM users", false},
      {"", false},
  };

  for (const auto& [query, expected] : cases) {
    if (client.isSelectQuery(query) == expected)
      continue;

    std::cerr << "Unexpected SELECT detection for query: " << query << '\n';
    return 1;
  }

  if (worm::databaseTypes.at("postgresql") != worm::DatabaseType::PostgreSQL ||
      worm::databaseTypes.at("mysql") != worm::DatabaseType::MySQL ||
      worm::databaseTypes.at("sqlite") != worm::DatabaseType::SQLite) {
    std::cerr << "Database type mapping is incorrect.\n";
    return 1;
  }

  return 0;
}
