#pragma once

#include <connection/client.hpp>
#include <connection/configuration.hpp>
#include <core/model/entity-metadata.hpp>
#include <core/persistence/registry.hpp>
#include <core/persistence/repository.hpp>
#include <core/query/query-builder.hpp>
#include <errors/concurrent-access-exception.hpp>
#include <errors/invalid-arg-exception.hpp>
#include <utils/dependency-injection.hpp>

#include <memory>
#include <thread>
#include <typeindex>
#include <unordered_map>
#include <utility>

namespace worm::context
{
  class PersistenceContext final
  {
  public:
    explicit PersistenceContext(const connection::ConnectionConfig& connectionConfig)
      : connectionConfig_(connectionConfig),
        client_(connection::makeClient(connectionConfig_, worm::DependencyInjector<connection::DatabaseType>().get())),
        registry_(std::make_shared<core::Registry>()),
        queryBuilder_()
    {}

    explicit PersistenceContext(const connection::ConnectionConfig& connectionConfig,
      std::shared_ptr<connection::Client> client,
      const core::QueryBuilder& queryBuilder)
      : connectionConfig_(connectionConfig),
        client_(std::move(client)),
        registry_(std::make_shared<core::Registry>()),
        queryBuilder_(queryBuilder)
    {
      if (!client_) {
        throw worm::InvalidArgException("PersistenceContext requires a valid client.");
      }
    }

    PersistenceContext(const PersistenceContext&) = delete;
    PersistenceContext& operator=(const PersistenceContext&) = delete;
    PersistenceContext(PersistenceContext&&) = delete;
    PersistenceContext& operator=(PersistenceContext&&) = delete;

    [[nodiscard]]
    const connection::ConnectionConfig& connectionConfig() const
    {
      return connectionConfig_;
    }

    [[nodiscard]]
    std::shared_ptr<connection::Client> client() const
    {
      ensureThreadAffinity();
      return client_;
    }

    [[nodiscard]]
    std::shared_ptr<core::Registry> registry() const
    {
      ensureThreadAffinity();
      return registry_;
    }

    template <core::PersistableEntity T>
    [[nodiscard]]
    core::InstanceRegistry<T>& instances() const
    {
      ensureThreadAffinity();
      return registry_->instances<T>();
    }

    template <core::PersistableEntity T>
    [[nodiscard]]
    const core::Repository<T>& repository() const
    {
      ensureThreadAffinity();
      const auto index = std::type_index(typeid(T));

      auto repository = repositories_.find(index);
      if (repository == repositories_.end()) {
        repository =
          repositories_.emplace(index, std::make_shared<core::Repository<T>>(client_, queryBuilder_, registry_)).first;
      }

      return *std::static_pointer_cast<core::Repository<T>>(repository->second);
    }

  private:
    void ensureThreadAffinity() const
    {
      if (std::this_thread::get_id() != ownerThread_) {
        throw worm::ConcurrentAccessException(
          "PersistenceContext accessed from a different thread than it was created on.");
      }
    }

    const connection::ConnectionConfig connectionConfig_;

    std::shared_ptr<connection::Client> client_;
    std::shared_ptr<core::Registry> registry_;
    const core::QueryBuilder queryBuilder_;
    const std::thread::id ownerThread_ = std::this_thread::get_id();
    mutable std::unordered_map<std::type_index, std::shared_ptr<void>> repositories_;
  };
} // namespace worm::context
