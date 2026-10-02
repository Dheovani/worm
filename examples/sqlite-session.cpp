#include "sqlite-example-support.hpp"

#include <connection/drivers/sqlite-client.hpp>
#include <core/persistence/session.hpp>
#include <core/query/query-builder.hpp>
#include <core/query/sql-builder.hpp>
#include <errors/worm-exception.hpp>
#include <reflection/field.hpp>

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <tuple>

namespace
{
  struct User
  {
    std::int64_t id{};
    std::string name;

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
      return std::tuple{worm::reflection::field("id", &User::id), worm::reflection::field("name", &User::name)};
    }
  };
} // namespace

int main()
try {
  const worm::examples::SqliteExampleDatabase database(
    "worm-sqlite-session.db",
    "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT NOT NULL)");

  const worm::connection::ConnectionConfig config{.dbname = database.path().string()};
  const auto client = std::make_shared<worm::connection::SqliteClient>(config);
  const worm::core::SqliteBuilder sqlBuilder;
  const worm::core::QueryBuilder queryBuilder{sqlBuilder};
  const worm::core::Session session{config, client, queryBuilder};
  const auto& users = session.repository<User>();

  const std::shared_ptr<User> created = users.insert({.id = 1, .name = "Ada"});
  const std::shared_ptr<User> firstLookup = users.find(std::int64_t{1});
  const std::shared_ptr<User> secondLookup = users.find(std::int64_t{1});

  firstLookup->name = "Ada Lovelace";
  const std::size_t pendingChanges = session.instances<User>().changedFieldCount(firstLookup->id);
  const std::uint64_t updatedRows = users.update(firstLookup->id, *firstLookup);

  std::cout << "same instance: " << std::boolalpha << (created == firstLookup && firstLookup == secondLookup) << "\n";
  std::cout << "changed fields before update: " << pendingChanges << "\n";
  std::cout << "updated rows: " << updatedRows << "\n";

  return created == firstLookup && firstLookup == secondLookup && pendingChanges == 1 && updatedRows == 1 &&
             !session.instances<User>().isDirty(firstLookup->id)
           ? 0
           : 1;
} catch (const worm::WormException& error) {
  std::cerr << "Worm error: " << error.what() << "\n";
  return 1;
} catch (const std::exception& error) {
  std::cerr << "Example setup error: " << error.what() << "\n";
  return 1;
}
