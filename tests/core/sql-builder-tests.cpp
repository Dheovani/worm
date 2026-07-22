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
