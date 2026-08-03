#include <connection/drivers/sqlite-client.hpp>
#include <core/persistence/repository.hpp>
#include <core/query/sql-builder.hpp>
#include <reflection/field.hpp>
#include <sqlite3.h>

#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>

namespace
{
  struct Person
  {
    std::int64_t id{};
    std::string name;
    std::int64_t active{};

    static constexpr worm::core::Table table() noexcept
    {
      return worm::core::Table{"people"};
    }

    static constexpr auto reflect() noexcept
    {
      return std::tuple{worm::reflection::field("id", &Person::id, {.primaryKey = true}),
        worm::reflection::field("name", &Person::name),
        worm::reflection::field("active", &Person::active)};
    }
  };

  bool prepareDatabase(const std::filesystem::path& databasePath)
  {
    sqlite3* connection = nullptr;
    if (sqlite3_open(databasePath.string().c_str(), &connection) != SQLITE_OK) {
      std::cerr << "Could not open the SQLite test database.\n";
      return false;
    }

    char* errorMessage = nullptr;
    const int result = sqlite3_exec(connection,
      "CREATE TABLE people (id INTEGER PRIMARY KEY, name TEXT NOT NULL, active INTEGER NOT NULL)",
      nullptr,
      nullptr,
      &errorMessage);

    if (result != SQLITE_OK) {
      std::cerr << "Could not prepare the SQLite test database: " << errorMessage << "\n";
      sqlite3_free(errorMessage);
      sqlite3_close(connection);
      return false;
    }

    sqlite3_close(connection);
    return true;
  }
} // namespace

int main()
try {
  using Client = worm::connection::SqliteClient;

  static_assert(std::is_base_of_v<worm::connection::Client, Client>);
  static_assert(std::is_final_v<Client>);
  static_assert(!std::is_copy_constructible_v<Client>);
  static_assert(!std::is_copy_assignable_v<Client>);

  const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "worm-sqlite-client-tests.db";

  std::filesystem::remove(databasePath);
  if (!prepareDatabase(databasePath)) {
    std::filesystem::remove(databasePath);
    return 1;
  }

  {
    const worm::connection::ConnectionConfig config{
      .dbname = databasePath.string(),
    };

    const auto client = std::make_shared<Client>(config);

    if (client->type() != worm::connection::DatabaseType::SQLite) {
      std::cerr << "SqliteClient returned the wrong database type.\n";
      return 1;
    }

    const worm::core::SqliteBuilder sqlBuilder;
    const worm::core::QueryBuilder queryBuilder{sqlBuilder};
    const worm::core::Repository<Person> repository{client, queryBuilder};

    const std::shared_ptr<Person> ada = repository.insert(Person{.id = 1, .name = "Ada", .active = 1});
    const std::shared_ptr<Person> grace = repository.insert(Person{.id = 2, .name = "Grace", .active = 0});

    if (!ada || ada->id != 1 || ada->name != "Ada" || !ada->active || !grace || grace->id != 2 ||
        grace->name != "Grace" || grace->active) {
      std::cerr << "Repository did not insert and hydrate entities through SqliteClient.\n";
      return 1;
    }

    const std::uint64_t updatedRows = repository.update(
      worm::core::Statement{"UPDATE people SET active = ? WHERE people.name = ?", {true, std::string{"Grace"}}});

    if (updatedRows != 1) {
      std::cerr << "SqliteClient did not report update feedback through Repository.\n";
      return 1;
    }

    const worm::core::Repository<Person> verificationRepository{client, queryBuilder};
    const std::shared_ptr<Person> updatedGrace = verificationRepository.find(std::int64_t{2});
    if (!updatedGrace || !updatedGrace->active) {
      std::cerr << "SqliteClient did not bind update parameters through Repository.\n";
      return 1;
    }

    repository.delete_(worm::core::Statement{"DELETE FROM people WHERE people.name = ?", {std::string{"Grace"}}});

    const worm::core::Repository<Person> deletionVerificationRepository{client, queryBuilder};
    if (deletionVerificationRepository.find(std::int64_t{2}) != nullptr) {
      std::cerr << "SqliteClient did not delete an entity through Repository.\n";
      return 1;
    }
  }

  std::filesystem::remove(databasePath);
  return 0;
} catch (const std::exception& error) {
  std::cerr << "SQLite repository integration failed: " << error.what() << "\n";
  return 1;
}
