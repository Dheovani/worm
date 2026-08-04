#pragma once

#include <connection/client.hpp>
#include <connection/configuration.hpp>
#include <core/model/entity-metadata.hpp>
#include <core/persistence/registry.hpp>
#include <core/persistence/repository.hpp>
#include <core/query/query-builder.hpp>

#include <memory>
#include <thread>
#include <typeindex>
#include <unordered_map>

namespace worm::context
{

  class Session final
  {
  public:
    explicit Session(const connection::ConnectionConfig& connectionConfig);

    explicit Session(const connection::ConnectionConfig& connectionConfig,
      std::shared_ptr<connection::Client> client,
      const core::QueryBuilder& queryBuilder);

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    Session(Session&&) = delete;
    Session& operator=(Session&&) = delete;

    [[nodiscard]]
    const connection::ConnectionConfig& connectionConfig() const;

    [[nodiscard]]
    std::shared_ptr<connection::Client> client() const;

    [[nodiscard]]
    std::shared_ptr<core::Registry> registry() const;

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
    void ensureThreadAffinity() const;

    const connection::ConnectionConfig connectionConfig_;

    std::shared_ptr<connection::Client> client_;
    std::shared_ptr<core::Registry> registry_;
    const core::QueryBuilder queryBuilder_;
    const std::thread::id ownerThread_ = std::this_thread::get_id();
    mutable std::unordered_map<std::type_index, std::shared_ptr<void>> repositories_;
  };

} // namespace worm::context
