#include <connection/drivers/sqlite-client.hpp>
#include <connection/transaction.hpp>
#include <core/persistence/repository.hpp>
#include <core/query/filter.hpp>
#include <core/query/predicate.hpp>
#include <core/query/query-builder.hpp>
#include <core/query/sql-builder.hpp>
#include <errors/worm-exception.hpp>
#include <reflection/field.hpp>
#include <sqlite3.h>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
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

    static constexpr auto reflect() noexcept
    {
      return std::tuple{
        worm::reflection::field("id", &User::id, {.primaryKey = true}),
        worm::reflection::field("name", &User::name),
        worm::reflection::field("email", &User::email)};
    }
  };

  class ExampleDatabase final
  {
  public:
    ExampleDatabase()
      : path_(std::filesystem::temp_directory_path() / "worm-sqlite-quick-start.db")
    {
      std::filesystem::remove(path_);

      sqlite3* connection = nullptr;
      if (sqlite3_open(path_.string().c_str(), &connection) != SQLITE_OK) {
        throw std::runtime_error("Could not create the example SQLite database.");
      }

      char* errorMessage = nullptr;
      const int result = sqlite3_exec(connection,
        "CREATE TABLE users ("
        "id INTEGER PRIMARY KEY, name TEXT NOT NULL, email TEXT NULL)",
        nullptr,
        nullptr,
        &errorMessage);

      if (result != SQLITE_OK) {
        const std::string message = errorMessage != nullptr ? errorMessage : "Could not create the users table.";
        sqlite3_free(errorMessage);
        sqlite3_close(connection);
        throw std::runtime_error(message);
      }

      sqlite3_close(connection);
    }

    ~ExampleDatabase() noexcept
    {
      std::error_code error;
      std::filesystem::remove(path_, error);
    }

    [[nodiscard]]
    const std::filesystem::path& path() const noexcept
    {
      return path_;
    }

  private:
    std::filesystem::path path_;
  };
} // namespace

int main()
try {
  const ExampleDatabase database;
  const worm::connection::ConnectionConfig config{
    .dbname = database.path().string(),
  };

  const auto client = std::make_shared<worm::connection::SqliteClient>(config);
  const worm::core::SqliteBuilder sqlBuilder;
  const worm::core::QueryBuilder queryBuilder{sqlBuilder};
  const auto registry = std::make_shared<worm::core::Registry>();
  const worm::core::Repository<User> users{client, queryBuilder, registry};

  const std::shared_ptr<User> ada = users.insert({
    .id = 1,
    .name = "Ada",
    .email = "ada@example.com",
  });

  ada->name = "Ada Lovelace";
  const std::uint64_t updatedRows = users.update(ada->id, *ada);

  const worm::core::Statement namedAda = queryBuilder.selectAll(
    {User::table().name()}, {}, worm::core::Filter{worm::core::Predicate::equal("users.name", ada->name)});
  const std::vector<std::shared_ptr<User>> matches = users.findAll(namedAda);

  {
    auto transaction = client->beginTransaction();
    static_cast<void>(users.insert({
      .id = 2,
      .name = "Grace Hopper",
      .email = std::nullopt,
    }));
    transaction.commit();
  }

  const std::shared_ptr<User> grace = users.find(std::int64_t{2});
  users.delete_(ada->id);

  std::cout << "updated rows: " << updatedRows << "\n";
  std::cout << "Ada matches: " << matches.size() << "\n";
  std::cout << "committed user: " << grace->name << "\n";
  return 0;
} catch (const worm::WormException& error) {
  std::cerr << "Worm error: " << error.what() << "\n";
  return 1;
} catch (const std::exception& error) {
  std::cerr << "Example setup error: " << error.what() << "\n";
  return 1;
}
