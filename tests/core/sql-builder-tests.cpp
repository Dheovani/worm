#include <core/sql-builder.hpp>

#include <core/predicate.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
  int assertFactoryReturnsBuilder(
    const std::filesystem::path& root,
    std::string_view databaseType)
  {
    {
      std::ofstream envFile(root / ".env");
      envFile << "database_type=" << databaseType << '\n';
    }

    const auto builder = worm::core::getSqlBuilder();

    if (databaseType == "postgresql" && dynamic_cast<worm::core::PgBuilder*>(builder.get()) == nullptr) {
      std::cerr << "SqlBuilder factory did not return PgBuilder.\n";
      return 1;
    }

    if (databaseType == "mysql" && dynamic_cast<worm::core::MySqlBuilder*>(builder.get()) == nullptr) {
      std::cerr << "SqlBuilder factory did not return MySqlBuilder.\n";
      return 1;
    }

    if (databaseType == "sqlite" && dynamic_cast<worm::core::SqliteBuilder*>(builder.get()) == nullptr) {
      std::cerr << "SqlBuilder factory did not return SqliteBuilder.\n";
      return 1;
    }

    return 0;
  }
} // namespace

int main()
{
  using worm::core::Comparison;
  using worm::core::Field;
  using worm::core::Filter;
  using worm::core::Join;
  using worm::core::MySqlBuilder;
  using worm::core::OrderDirection;
  using worm::core::Ordering;
  using worm::core::PgBuilder;
  using worm::core::Predicate;
  using worm::core::Relation;
  using worm::core::Source;
  using worm::core::SqliteBuilder;

  const PgBuilder pgBuilder;
  const MySqlBuilder mySqlBuilder;
  const SqliteBuilder sqliteBuilder;

  const Source users{"users", "u"};
  const Source orders{"orders", "o"};
  const std::vector<Field> fields{
    Field{"id", users},
    Field{"total", orders},
  };
  const std::vector<Relation> relations{
    Relation{
      Join::Inner,
      users,
      orders,
      Predicate::compare("u.id", Comparison::Equal, std::int64_t{7})},
  };
  const Filter filter{Predicate::equal("u.active", true)};
  const std::vector<Ordering> ordering{
    Ordering{"o.total", OrderDirection::Descending},
  };
  const std::string select = pgBuilder.select(fields, users, relations, filter, ordering);

  if (select !=
      "select u.id,o.total from users u inner join orders o on (u.id = $1)"
      " where u.active = $2 order by o.total desc") {
    std::cerr << "Select builder did not render joined source, filter, or ordering correctly.\n";
    return 1;
  }

  const std::string mySqlSelect = mySqlBuilder.select(fields, users, relations, filter, ordering);
  const std::string sqliteSelect = sqliteBuilder.select(fields, users, relations, filter, ordering);

  if (mySqlSelect !=
        "select u.id,o.total from users u inner join orders o on (u.id = ?)"
        " where u.active = ? order by o.total desc" ||
      sqliteSelect !=
        "select u.id,o.total from users u inner join orders o on (u.id = ?)"
        " where u.active = ? order by o.total desc") {
    std::cerr << "Question mark dialect placeholders were rendered incorrectly.\n";
    return 1;
  }

  const Source payments{"payments", "p"};
  const std::vector<Relation> multipleRelations{
    Relation{
      Join::Inner,
      users,
      orders,
      Predicate::compare("u.id", Comparison::Equal, std::int64_t{7})},
    Relation{
      Join::Left,
      orders,
      payments,
      Predicate::compare("o.id", Comparison::Equal, std::int64_t{9})},
  };

  const std::string multiJoinSelect = pgBuilder.select(fields, users, multipleRelations, filter, ordering);
  if (multiJoinSelect !=
      "select u.id,o.total from users u inner join orders o on (u.id = $1)"
      " left join payments p on (o.id = $2) where u.active = $3 order by o.total desc") {
    std::cerr << "Select builder did not separate multiple joins or number parameters correctly.\n";
    return 1;
  }

  const std::vector<std::pair<std::string, worm::core::Parameter>> insertColumns{
    {"name", std::string{"Ada"}},
    {"active", true},
  };

  if (pgBuilder.insert(users, insertColumns) != "insert into users(name,active) values ($1,$2)" ||
      mySqlBuilder.insert(users, insertColumns) != "insert into users(name,active) values (?,?)" ||
      sqliteBuilder.insert(users, insertColumns) != "insert into users(name,active) values (?,?)") {
    std::cerr << "Insert builder did not render placeholders correctly.\n";
    return 1;
  }

  const Source archivedUsers{"archived_users"};
  const std::vector<std::string> targetColumns{
    "id",
    "total",
  };

  if (pgBuilder.insertFromSelect(archivedUsers, targetColumns, select) !=
      "insert into archived_users(id,total) "
      "select u.id,o.total from users u inner join orders o on (u.id = $1)"
      " where u.active = $2 order by o.total desc") {
    std::cerr << "Insert from raw select query was not rendered correctly.\n";
    return 1;
  }

  if (pgBuilder.insertFromSelect(archivedUsers, targetColumns, fields, users, relations, filter, ordering) !=
      "insert into archived_users(id,total) "
      "select u.id,o.total from users u inner join orders o on (u.id = $1)"
      " where u.active = $2 order by o.total desc") {
    std::cerr << "Insert from structured select query was not rendered correctly.\n";
    return 1;
  }

  const std::string pgUpdate = pgBuilder.update(users, insertColumns, filter);
  const std::string mySqlUpdate = mySqlBuilder.update(users, insertColumns, filter);
  const std::string sqliteUpdate = sqliteBuilder.update(users, insertColumns, filter);
  if (pgUpdate != "update users u set name = $1,active = $2 where u.active = $3" ||
      mySqlUpdate != "update users u set name = ?,active = ? where u.active = ?" ||
      sqliteUpdate != "update users u set name = ?,active = ? where u.active = ?") {
    std::cerr << "Update builder did not render placeholders or filter correctly.\n";
    return 1;
  }

  const std::string pgDelete = pgBuilder.delete_(users, filter);
  const std::string mySqlDelete = mySqlBuilder.delete_(users, filter);
  const std::string sqliteDelete = sqliteBuilder.delete_(users, filter);
  if (pgDelete != "delete from users u where u.active = $1" ||
      mySqlDelete != "delete from users u where u.active = ?" ||
      sqliteDelete != "delete from users u where u.active = ?") {
    std::cerr << "Delete builder did not render placeholders or filter correctly.\n";
    return 1;
  }

  const std::filesystem::path originalPath = std::filesystem::current_path();
  const std::filesystem::path root = std::filesystem::temp_directory_path() / "worm-sql-builder-tests";

  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  std::filesystem::current_path(root);

  int result = 0;
  try {
    result = assertFactoryReturnsBuilder(root, "postgresql");
    if (result == 0) {
      result = assertFactoryReturnsBuilder(root, "mysql");
    }
    if (result == 0) {
      result = assertFactoryReturnsBuilder(root, "sqlite");
    }
  } catch (const std::exception& error) {
    std::cerr << "SqlBuilder factory test failed: " << error.what() << '\n';
    result = 1;
  }

  std::filesystem::current_path(originalPath);
  std::filesystem::remove_all(root);

  return result;
}
