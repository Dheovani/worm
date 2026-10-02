#include "sqlite-example-support.hpp"

#include <connection/drivers/sqlite-client.hpp>
#include <connection/transaction.hpp>
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
    "worm-sqlite-transactions.db",
    "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT NOT NULL)");

  const worm::connection::ConnectionConfig config{.dbname = database.path().string()};
  const auto client = std::make_shared<worm::connection::SqliteClient>(config);
  const worm::core::SqliteBuilder sqlBuilder;
  const worm::core::QueryBuilder queryBuilder{sqlBuilder};

  {
    const worm::core::Repository<User> users{client, queryBuilder};
    auto transaction = client->beginTransaction();
    static_cast<void>(users.insert({.id = 1, .name = "rolled back"}));
    transaction.rollback();
  }

  const worm::core::Repository<User> afterRollback{client, queryBuilder};
  if (afterRollback.find(std::int64_t{1}) != nullptr) {
    std::cerr << "Explicit rollback kept the inserted row.\n";
    return 1;
  }

  {
    const worm::core::Repository<User> users{client, queryBuilder};
    auto transaction = client->beginTransaction();
    static_cast<void>(users.insert({.id = 2, .name = "committed"}));
    transaction.commit();
  }

  const worm::core::Repository<User> afterCommit{client, queryBuilder};
  const std::shared_ptr<User> committed = afterCommit.find(std::int64_t{2});
  if (committed == nullptr) {
    std::cerr << "Commit did not preserve the inserted row.\n";
    return 1;
  }

  {
    const worm::core::Repository<User> users{client, queryBuilder};
    auto transaction = client->beginTransaction();
    static_cast<void>(users.insert({.id = 3, .name = "scope rollback"}));
    static_cast<void>(transaction);
  }

  const worm::core::Repository<User> afterScope{client, queryBuilder};
  const bool scopeRolledBack = afterScope.find(std::int64_t{3}) == nullptr;

  std::cout << "explicit rollback: ok\n";
  std::cout << "explicit commit: " << committed->name << "\n";
  std::cout << "scope rollback: " << (scopeRolledBack ? "ok" : "failed") << "\n";
  return scopeRolledBack ? 0 : 1;
} catch (const worm::WormException& error) {
  std::cerr << "Worm error: " << error.what() << "\n";
  return 1;
} catch (const std::exception& error) {
  std::cerr << "Example setup error: " << error.what() << "\n";
  return 1;
}
