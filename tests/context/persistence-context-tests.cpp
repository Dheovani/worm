#include <context/persistence-context.hpp>

#include <errors/concurrent-access-exception.hpp>
#include <reflection/field.hpp>

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
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

  class TestClient final : public worm::connection::Client
  {
  public:
    [[nodiscard]]
    worm::connection::DatabaseType type() const noexcept override
    {
      return worm::connection::DatabaseType::SQLite;
    }

  private:
    void beginTransactionImpl() override {}

    void rollbackTransactionImpl() override {}

    void commitTransactionImpl() override {}

    [[nodiscard]]
    worm::core::ResultSet executeImpl(const worm::core::Statement&) override
    {
      return {};
    }
  };

} // namespace

int main()
{
  std::shared_ptr<worm::connection::Client> retainedClient;
  std::weak_ptr<worm::connection::Client> clientLifetime;
  {
    const auto client = std::make_shared<TestClient>();
    const worm::core::SqliteBuilder sqlBuilder;
    const worm::core::QueryBuilder queryBuilder{sqlBuilder};
    const worm::context::PersistenceContext context({}, client, queryBuilder);

    retainedClient = context.client();
    clientLifetime = retainedClient;
    if (retainedClient->type() != worm::connection::DatabaseType::SQLite) {
      std::cerr << "PersistenceContext created a client for the wrong database.\n";
      return 1;
    }

    const auto& firstRepository = context.repository<User>();
    const auto& secondRepository = context.repository<User>();
    if (&firstRepository != &secondRepository) {
      std::cerr << "PersistenceContext did not reuse its typed repository.\n";
      return 1;
    }

    context.instances<User>().put(7, User{.id = 7, .name = "Ada"});
    if (!context.registry()->instances<User>().has(7)) {
      std::cerr << "PersistenceContext did not share its registry with entity access.\n";
      return 1;
    }

    bool foreignThreadRejected = false;
    std::thread foreignAccess([&] {
      try {
        static_cast<void>(context.registry());
      } catch (const worm::ConcurrentAccessException&) {
        foreignThreadRejected = true;
      }
    });
    foreignAccess.join();

    if (!foreignThreadRejected) {
      std::cerr << "PersistenceContext accepted access from a foreign thread.\n";
      return 1;
    }
  }

  if (clientLifetime.expired()) {
    std::cerr << "Client did not survive while an external shared owner remained.\n";
    return 1;
  }

  retainedClient.reset();
  if (!clientLifetime.expired()) {
    std::cerr << "Client outlived all of its shared owners.\n";
    return 1;
  }

  return 0;
}
