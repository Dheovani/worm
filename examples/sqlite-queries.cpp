#include "sqlite-example-support.hpp"

#include <connection/drivers/sqlite-client.hpp>
#include <core/persistence/repository.hpp>
#include <core/query/criteria.hpp>
#include <core/query/query-builder.hpp>
#include <core/query/sql-builder.hpp>
#include <errors/worm-exception.hpp>
#include <reflection/field.hpp>

#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace
{
  struct User
  {
    std::int64_t id{};
    std::string name;
    std::optional<std::string> email;

    static constexpr worm::core::Table table() noexcept
    {
      return worm::core::Table{"users"};
    }

    static constexpr worm::core::PrimaryKey primaryKey() noexcept
    {
      return worm::core::PrimaryKey{"pk_users", {worm::core::Column{"id", table()}}};
    }

    static constexpr auto reflect() noexcept
    {
      return std::tuple{worm::reflection::field("id", &User::id),
        worm::reflection::field("name", &User::name),
        worm::reflection::field("email", &User::email)};
    }
  };
} // namespace

int main()
try {
  const worm::examples::SqliteExampleDatabase database(
    "worm-sqlite-queries.db",
    "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT NOT NULL, email TEXT NULL)");

  const worm::connection::ConnectionConfig config{.dbname = database.path().string()};
  const auto client = std::make_shared<worm::connection::SqliteClient>(config);
  const worm::core::SqliteBuilder sqlBuilder;
  const worm::core::QueryBuilder queryBuilder{sqlBuilder};
  const worm::core::Repository<User> users{client, queryBuilder};

  static_cast<void>(users.insert(
    std::vector<User>{
      {.id = 1, .name = "Ada Lovelace", .email = "ada@example.com"},
      {.id = 2, .name = "Grace Hopper", .email = std::nullopt},
      {.id = 3, .name = "Barbara Liskov", .email = "barbara@example.com"},
    }));

  worm::core::Criteria criteria;
  criteria.where(worm::core::Filter{worm::core::Predicate::in("u.id", {std::int64_t{1}, std::int64_t{3}})})
    .orderBy(worm::core::Ordering{"u.name", worm::core::OrderDirection::Descending})
    .paginate(worm::core::Pagination{2});

  const worm::core::Statement statement = queryBuilder.selectAll({User::table().name(), "u"}, criteria);
  const std::vector<std::shared_ptr<User>> matches = users.findAll(statement);

  std::cout << statement.sql << "\n";
  std::cout << "bound parameters: " << statement.parameters.size() << "\n";
  for (const auto& user : matches) {
    std::cout << user->id << ": " << user->name << "\n";
  }

  return matches.size() == 2 ? 0 : 1;
} catch (const worm::WormException& error) {
  std::cerr << "Worm error: " << error.what() << "\n";
  return 1;
} catch (const std::exception& error) {
  std::cerr << "Example setup error: " << error.what() << "\n";
  return 1;
}
