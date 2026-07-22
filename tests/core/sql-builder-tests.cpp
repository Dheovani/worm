#include <core/sql-builder.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

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

    const auto builder = worm::core::getBuilder();

    if (databaseType == "postgresql" && dynamic_cast<worm::core::PgBuilder*>(builder.get()) == nullptr) {
      std::cerr << "Builder factory did not return PgBuilder.\n";
      return 1;
    }

    if (databaseType == "mysql" && dynamic_cast<worm::core::MySqlBuilder*>(builder.get()) == nullptr) {
      std::cerr << "Builder factory did not return MySqlBuilder.\n";
      return 1;
    }

    if (databaseType == "sqlite" && dynamic_cast<worm::core::SqliteBuilder*>(builder.get()) == nullptr) {
      std::cerr << "Builder factory did not return SqliteBuilder.\n";
      return 1;
    }

    return 0;
  }
} // namespace

int main()
{
  using worm::core::Comparison;
  using worm::core::MySqlBuilder;
  using worm::core::PgBuilder;
  using worm::core::SqliteBuilder;

  const PgBuilder pgBuilder;
  const MySqlBuilder mySqlBuilder;
  const SqliteBuilder sqliteBuilder;

  const auto expression = pgBuilder.compare("id", Comparison::Equal, std::int64_t{7});
  if (pgBuilder.renderExpression(expression) != "id = $1" ||
      mySqlBuilder.renderExpression(expression) != "id = ?" ||
      sqliteBuilder.renderExpression(expression) != "id = ?") {
    std::cerr << "Dialect placeholders were rendered incorrectly.\n";
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
    std::cerr << "Builder factory test failed: " << error.what() << '\n';
    result = 1;
  }

  std::filesystem::current_path(originalPath);
  std::filesystem::remove_all(root);

  return result;
}
