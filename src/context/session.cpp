#include <context/session.hpp>

#include <errors/concurrent-access-exception.hpp>
#include <errors/invalid-arg-exception.hpp>

#include <utility>

namespace worm::context
{

  Session::Session(const connection::ConnectionConfig& connectionConfig)
    : connectionConfig_(connectionConfig),
      client_(connection::makeClient(connectionConfig_, DependencyInjector<connection::DatabaseType>::get())),
      registry_(std::make_shared<core::Registry>()),
      queryBuilder_()
  {}
  
  Session::Session(const connection::ConnectionConfig& connectionConfig,
    std::shared_ptr<connection::Client> client,
    const core::QueryBuilder& queryBuilder)
    : connectionConfig_(connectionConfig),
      client_(std::move(client)),
      registry_(std::make_shared<core::Registry>()),
      queryBuilder_(queryBuilder)
  {
    if (!client_) {
      throw InvalidArgException("Session requires a valid client.");
    }
  }

  const connection::ConnectionConfig& Session::connectionConfig() const
  {
    ensureThreadAffinity();
    return connectionConfig_;
  }

  std::shared_ptr<connection::Client> Session::client() const
  {
    ensureThreadAffinity();
    return client_;
  }

  std::shared_ptr<core::Registry> Session::registry() const
  {
    ensureThreadAffinity();
    return registry_;
  }

  void Session::ensureThreadAffinity() const
  {
    if (std::this_thread::get_id() != ownerThread_) {
      throw ConcurrentAccessException("Session accessed from a different thread than it was created on.");
    }
  }

} // namespace worm::context
