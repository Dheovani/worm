#include "sqlite-example-support.hpp"

#include <connection/drivers/sqlite-client.hpp>
#include <core/model/relationship.hpp>
#include <core/persistence/repository.hpp>
#include <core/query/criteria.hpp>
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
  struct Profile
  {
    std::int64_t id{};
    std::int64_t userId{};

    static constexpr worm::core::Table table() noexcept
    {
      return worm::core::Table{"profiles"};
    }

    static constexpr worm::core::PrimaryKey primaryKey() noexcept
    {
      return worm::core::PrimaryKey{"pk_profiles", {worm::core::Column{"id", table()}}};
    }

    static constexpr auto reflect() noexcept
    {
      return std::tuple{worm::reflection::field("id", &Profile::id),
        worm::reflection::field("user_id", &Profile::userId)};
    }
  };

  struct Post
  {
    std::int64_t id{};
    std::int64_t userId{};

    static constexpr worm::core::Table table() noexcept
    {
      return worm::core::Table{"posts"};
    }

    static constexpr worm::core::PrimaryKey primaryKey() noexcept
    {
      return worm::core::PrimaryKey{"pk_posts", {worm::core::Column{"id", table()}}};
    }

    static constexpr auto reflect() noexcept
    {
      return std::tuple{worm::reflection::field("id", &Post::id), worm::reflection::field("user_id", &Post::userId)};
    }
  };

  struct Role
  {
    std::int64_t id{};
    std::string name;

    static constexpr worm::core::Table table() noexcept
    {
      return worm::core::Table{"roles"};
    }

    static constexpr worm::core::PrimaryKey primaryKey() noexcept
    {
      return worm::core::PrimaryKey{"pk_roles", {worm::core::Column{"id", table()}}};
    }

    static constexpr auto reflect() noexcept
    {
      return std::tuple{worm::reflection::field("id", &Role::id), worm::reflection::field("name", &Role::name)};
    }
  };

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

    static constexpr auto relationships() noexcept
    {
      return std::tuple{worm::core::oneToOne<User, Profile>("profile", "id", "user_id"),
        worm::core::oneToMany<User, Post>("posts", "id", "user_id"),
        worm::core::manyToMany<User, Role>("roles", "user_roles", "id", "user_id", "role_id", "id")};
    }
  };
} // namespace

int main()
try {
  const worm::examples::SqliteExampleDatabase database(
    "worm-sqlite-relationships.db",
    "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT NOT NULL);"
    "CREATE TABLE profiles (id INTEGER PRIMARY KEY, user_id INTEGER NOT NULL UNIQUE);"
    "CREATE TABLE posts (id INTEGER PRIMARY KEY, user_id INTEGER NOT NULL);"
    "CREATE TABLE roles (id INTEGER PRIMARY KEY, name TEXT NOT NULL);"
    "CREATE TABLE user_roles (user_id INTEGER NOT NULL, role_id INTEGER NOT NULL);"
    "INSERT INTO users VALUES (1, 'Ada Lovelace');"
    "INSERT INTO profiles VALUES (1, 1);"
    "INSERT INTO posts VALUES (1, 1);"
    "INSERT INTO roles VALUES (1, 'admin');"
    "INSERT INTO user_roles VALUES (1, 1)");

  const worm::connection::ConnectionConfig config{.dbname = database.path().string()};
  const auto client = std::make_shared<worm::connection::SqliteClient>(config);
  const worm::core::SqliteBuilder sqlBuilder;
  const worm::core::QueryBuilder queryBuilder{sqlBuilder};
  const worm::core::Repository<User> users{client, queryBuilder};

  constexpr auto relationships = User::relationships();
  worm::core::Criteria criteria;
  criteria.include(std::get<0>(relationships), "u", "p")
    .include(std::get<1>(relationships), "u", "po")
    .include(std::get<2>(relationships), "u", "ur", "r")
    .where(worm::core::Filter{worm::core::Predicate::equal("r.name", std::string{"admin"})});

  const worm::core::Statement statement = queryBuilder.selectAll({User::table().name(), "u"}, criteria);
  const std::vector<std::shared_ptr<User>> matches = users.findAll(statement);

  std::cout << statement.sql << "\n";
  std::cout << "matched users: " << matches.size() << "\n";
  return matches.size() == 1 && matches.front()->name == "Ada Lovelace" ? 0 : 1;
} catch (const worm::WormException& error) {
  std::cerr << "Worm error: " << error.what() << "\n";
  return 1;
} catch (const std::exception& error) {
  std::cerr << "Example setup error: " << error.what() << "\n";
  return 1;
}
