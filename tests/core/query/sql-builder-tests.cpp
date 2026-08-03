#include <core/query/sql-builder.hpp>

#include <core/query/predicate.hpp>
#include <errors/sql-build-exception.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
  class CurrentPathGuard
  {
  public:
    CurrentPathGuard()
      : originalPath_(std::filesystem::current_path())
    {}

    ~CurrentPathGuard()
    {
      std::error_code error;
      std::filesystem::current_path(originalPath_, error);
    }

    CurrentPathGuard(const CurrentPathGuard&) = delete;
    CurrentPathGuard& operator=(const CurrentPathGuard&) = delete;

  private:
    std::filesystem::path originalPath_;
  };

  std::filesystem::path uniqueTempDirectory()
  {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();

    return std::filesystem::temp_directory_path() / ("worm-sql-builder-tests-" + std::to_string(now));
  }

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
  const worm::core::Statement select = pgBuilder.select(fields, users, relations, filter, ordering);

  if (select.sql !=
      "select u.id,o.total from users u inner join orders o on (u.id = $1)"
      " where u.active = $2 order by o.total desc") {
    std::cerr << "Select builder did not render joined source, filter, or ordering correctly.\n";
    return 1;
  }

  if (select.parameters != std::vector<worm::core::Parameter>{std::int64_t{7}, true}) {
    std::cerr << "Select builder did not preserve relation and filter parameters.\n";
    return 1;
  }

  const worm::core::Statement mySqlSelect = mySqlBuilder.select(fields, users, relations, filter, ordering);
  const worm::core::Statement sqliteSelect = sqliteBuilder.select(fields, users, relations, filter, ordering);

  if (mySqlSelect.sql !=
        "select u.id,o.total from users u inner join orders o on (u.id = ?)"
        " where u.active = ? order by o.total desc" ||
      sqliteSelect.sql !=
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

  const worm::core::Statement multiJoinSelect = pgBuilder.select(fields, users, multipleRelations, filter, ordering);
  if (multiJoinSelect.sql !=
      "select u.id,o.total from users u inner join orders o on (u.id = $1)"
      " left join payments p on (o.id = $2) where u.active = $3 order by o.total desc") {
    std::cerr << "Select builder did not separate multiple joins or number parameters correctly.\n";
    return 1;
  }

  if (multiJoinSelect.parameters != std::vector<worm::core::Parameter>{std::int64_t{7}, std::int64_t{9}, true}) {
    std::cerr << "Select builder did not preserve multiple relation parameters before filter parameters.\n";
    return 1;
  }

  const std::vector<std::pair<std::string, worm::core::Parameter>> insertColumns{
    {"name", std::string{"Ada"}},
    {"active", true},
  };

  const worm::core::Statement pgInsert = pgBuilder.insert(users, insertColumns);
  const worm::core::Statement mySqlInsert = mySqlBuilder.insert(users, insertColumns);
  const worm::core::Statement sqliteInsert = sqliteBuilder.insert(users, insertColumns);
  if (pgInsert.sql != "insert into users(name,active) values ($1,$2)" ||
      mySqlInsert.sql != "insert into users(name,active) values (?,?)" ||
      sqliteInsert.sql != "insert into users(name,active) values (?,?)") {
    std::cerr << "Insert builder did not render placeholders correctly.\n";
    return 1;
  }

  if (pgInsert.parameters != std::vector<worm::core::Parameter>{std::string{"Ada"}, true}) {
    std::cerr << "Insert builder did not preserve column parameters.\n";
    return 1;
  }

  const Source archivedUsers{"archived_users"};
  const std::vector<std::string> targetColumns{
    "id",
    "total",
  };

  const worm::core::Statement rawInsertFromSelect = pgBuilder.insertFromSelect(archivedUsers, targetColumns, select);
  if (rawInsertFromSelect.sql !=
      "insert into archived_users(id,total) "
      "select u.id,o.total from users u inner join orders o on (u.id = $1)"
      " where u.active = $2 order by o.total desc") {
    std::cerr << "Insert from raw select query was not rendered correctly.\n";
    return 1;
  }

  if (rawInsertFromSelect.parameters != select.parameters) {
    std::cerr << "Insert from raw select query did not preserve source parameters.\n";
    return 1;
  }

  const worm::core::Statement structuredInsertFromSelect =
    pgBuilder.insertFromSelect(archivedUsers, targetColumns, fields, users, relations, filter, ordering);
  if (structuredInsertFromSelect.sql !=
      "insert into archived_users(id,total) "
      "select u.id,o.total from users u inner join orders o on (u.id = $1)"
      " where u.active = $2 order by o.total desc") {
    std::cerr << "Insert from structured select query was not rendered correctly.\n";
    return 1;
  }

  if (structuredInsertFromSelect.parameters != select.parameters) {
    std::cerr << "Insert from structured select query did not preserve source parameters.\n";
    return 1;
  }

  const worm::core::Statement pgUpdate = pgBuilder.update(users, insertColumns, filter);
  const worm::core::Statement mySqlUpdate = mySqlBuilder.update(users, insertColumns, filter);
  const worm::core::Statement sqliteUpdate = sqliteBuilder.update(users, insertColumns, filter);
  if (pgUpdate.sql != "update users u set name = $1,active = $2 where u.active = $3" ||
      mySqlUpdate.sql != "update users u set name = ?,active = ? where u.active = ?" ||
      sqliteUpdate.sql != "update users as u set name = ?,active = ? where u.active = ?") {
    std::cerr << "Update builder did not render placeholders or filter correctly.\n";
    return 1;
  }

  if (pgUpdate.parameters != std::vector<worm::core::Parameter>{std::string{"Ada"}, true, true}) {
    std::cerr << "Update builder did not preserve column parameters before filter parameters.\n";
    return 1;
  }

  bool emptyUpdateFailed = false;
  try {
    static_cast<void>(pgBuilder.update(users, {}, filter));
  } catch (const worm::SqlBuildException&) {
    emptyUpdateFailed = true;
  }

  if (!emptyUpdateFailed) {
    std::cerr << "Update builder accepted an update without columns.\n";
    return 1;
  }

  const worm::core::Statement pgDelete = pgBuilder.delete_(users, filter);
  const worm::core::Statement mySqlDelete = mySqlBuilder.delete_(users, filter);
  const worm::core::Statement sqliteDelete = sqliteBuilder.delete_(users, filter);
  if (pgDelete.sql != "delete from users u where u.active = $1" ||
      mySqlDelete.sql != "delete from users u where u.active = ?" ||
      sqliteDelete.sql != "delete from users as u where u.active = ?") {
    std::cerr << "Delete builder did not render placeholders or filter correctly.\n";
    return 1;
  }

  if (pgDelete.parameters != std::vector<worm::core::Parameter>{true}) {
    std::cerr << "Delete builder did not preserve filter parameters.\n";
    return 1;
  }

  const CurrentPathGuard currentPathGuard;
  const std::filesystem::path root = uniqueTempDirectory();

  std::error_code cleanupError;
  std::filesystem::remove_all(root, cleanupError);
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

  std::filesystem::current_path(std::filesystem::temp_directory_path(), cleanupError);
  std::filesystem::remove_all(root, cleanupError);

  return result;
}
