#include <connection/client.hpp>
#include <connection/transaction.hpp>
#include <core/persistence/repository.hpp>
#include <core/query/sql-builder.hpp>
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

    static constexpr auto reflect() noexcept
    {
      return std::tuple{
        worm::reflection::field("id", &User::id, {.primaryKey = true}), worm::reflection::field("name", &User::name)};
    }
  };

  class CountingClient final : public worm::connection::Client
  {
  public:
    explicit CountingClient(bool cacheResults = true)
      : Client(cacheResults)
    {}

    [[nodiscard]]
    worm::connection::DatabaseType type() const noexcept override
    {
      return worm::connection::DatabaseType::SQLite;
    }

    std::size_t executions{};

  private:
    void beginTransactionImpl() override {}

    void rollbackTransactionImpl() override {}

    void commitTransactionImpl() override {}

    worm::core::ResultSet executeImpl(const worm::core::Statement& statement) override
    {
      ++executions;

      if (!worm::core::isSelect(statement.sql)) {
        return worm::core::ResultSet{std::uint64_t{1}};
      }

      const std::int64_t id = std::get<std::int64_t>(statement.parameters.front());
      return worm::core::ResultSet{{{{{"id", id}, {"name", std::string{"User "} + std::to_string(id)}}}}};
    }
  };
} // namespace

int main()
{
  const worm::core::SqliteBuilder sqlBuilder;
  const worm::core::QueryBuilder queryBuilder{sqlBuilder};
  const worm::core::Statement first{"SELECT id, name FROM users WHERE id = ?", {std::int64_t{1}}};
  const worm::core::Statement second{"SELECT id, name FROM users WHERE id = ?", {std::int64_t{2}}};

  const auto uncachedClient = std::make_shared<CountingClient>(false);
  const worm::core::Repository<User> uncachedRepository{uncachedClient, queryBuilder};
  static_cast<void>(uncachedRepository.findAll(first));
  static_cast<void>(uncachedRepository.findAll(first));
  if (uncachedClient->executions != 2) {
    std::cerr << "Client enabled result caching without an explicit opt-in.\n";
    return 1;
  }

  const auto client = std::make_shared<CountingClient>();
  const worm::core::Repository<User> repository{client, queryBuilder};

  static_cast<void>(repository.findAll(first));
  static_cast<void>(repository.findAll(first));
  if (client->executions != 1) {
    std::cerr << "Client did not reuse an identical cached SELECT.\n";
    return 1;
  }

  static_cast<void>(repository.findAll(second));
  if (client->executions != 2) {
    std::cerr << "Client cache ignored statement parameters.\n";
    return 1;
  }

  static_cast<void>(repository.insert(
    worm::core::Statement{"INSERT INTO users(id, name) VALUES (?, ?)", {std::int64_t{3}, std::string{"User 3"}}}));
  static_cast<void>(repository.findAll(first));
  if (client->executions != 4) {
    std::cerr << "Client did not invalidate cached results after a mutation.\n";
    return 1;
  }

  {
    auto transaction = client->beginTransaction();
    static_cast<void>(repository.findAll(first));
    static_cast<void>(repository.findAll(first));
    transaction.rollback();
  }

  if (client->executions != 6) {
    std::cerr << "Client reused cached results inside a transaction.\n";
    return 1;
  }

  static_cast<void>(repository.findAll(first));
  static_cast<void>(repository.findAll(first));
  if (client->executions != 7) {
    std::cerr << "Client did not rebuild its cache after a transaction.\n";
    return 1;
  }

  return 0;
}
