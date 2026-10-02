#include "sqlite-example-support.hpp"

#include <connection/drivers/sqlite-client.hpp>
#include <core/persistence/repository.hpp>
#include <core/query/query-builder.hpp>
#include <core/query/sql-builder.hpp>
#include <errors/worm-exception.hpp>
#include <reflection/field.hpp>

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace
{
  struct ActiveUser
  {
    std::int64_t id{};
    std::string name;

    static constexpr worm::core::View view() noexcept
    {
      return worm::core::View{"active_users"}.definedBy("SELECT id, name FROM users WHERE active = 1");
    }

    static constexpr auto reflect() noexcept
    {
      return std::tuple{worm::reflection::field("id", &ActiveUser::id),
        worm::reflection::field("name", &ActiveUser::name)};
    }
  };
} // namespace

int main()
try {
  static_assert(worm::core::QueryableView<ActiveUser>);
  static_assert(!worm::core::PersistableEntity<ActiveUser>);

  const worm::examples::SqliteExampleDatabase database(
    "worm-sqlite-view.db",
    "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT NOT NULL, active INTEGER NOT NULL);"
    "INSERT INTO users VALUES (1, 'Ada', 1);"
    "INSERT INTO users VALUES (2, 'Grace', 0);"
    "CREATE VIEW active_users AS SELECT id, name FROM users WHERE active = 1");

  const worm::connection::ConnectionConfig config{.dbname = database.path().string()};
  const auto client = std::make_shared<worm::connection::SqliteClient>(config);
  const worm::core::SqliteBuilder sqlBuilder;
  const worm::core::QueryBuilder queryBuilder{sqlBuilder};
  const worm::core::Repository<ActiveUser> activeUsers{client, queryBuilder};

  const worm::core::Statement statement = queryBuilder.selectAll({ActiveUser::view().name()});
  const std::vector<std::shared_ptr<ActiveUser>> users = activeUsers.findAll(statement);

  std::cout << ActiveUser::view().definition() << "\n";
  std::cout << "active users: " << users.size() << "\n";
  return users.size() == 1 && users.front()->name == "Ada" ? 0 : 1;
} catch (const worm::WormException& error) {
  std::cerr << "Worm error: " << error.what() << "\n";
  return 1;
} catch (const std::exception& error) {
  std::cerr << "Example setup error: " << error.what() << "\n";
  return 1;
}
