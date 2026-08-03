#pragma once

#include <core/model/entity-metadata.hpp>
#include <core/model/entity.hpp>
#include <errors/concurrent-access-exception.hpp>
#include <reflection/snapshot.hpp>

#include <map>
#include <memory>
#include <thread>
#include <typeindex>
#include <unordered_map>
#include <utility>

namespace worm::core
{

  template <PersistableEntity T>
  class InstanceRegistry final
  {
    using PrimaryKeyField = decltype(primary_key_field_of<T>());
    using PK = typename PrimaryKeyField::value_type;

  public:
    explicit InstanceRegistry()
      : instances_(),
        snapshots_()
    {}

    InstanceRegistry(const InstanceRegistry&) = delete;
    InstanceRegistry& operator=(const InstanceRegistry&) = delete;
    InstanceRegistry(InstanceRegistry&&) = delete;
    InstanceRegistry& operator=(InstanceRegistry&&) = delete;

    [[nodiscard]]
    bool has(const T& instance) const
    {
      return has(primaryKey_.get(instance));
    }

    [[nodiscard]]
    bool has(const PK& key) const
    {
      ensureThreadAffinity();
      return instances_.contains(key);
    }

    [[nodiscard]]
    std::shared_ptr<T> get(const PK& key) const
    {
      if (has(key))
        return instances_.at(key);

      return nullptr;
    }

    InstanceRegistry<T>& add(const PK& key, const T& entity)
    {
      ensureThreadAffinity();
      auto instance = std::make_shared<T>(entity);

      const auto [_, inserted] = instances_.insert({key, instance});
      if (inserted) {
        snapshots_.insert({key, reflection::make_snapshot(*instance)});
      }

      return *this;
    }

    InstanceRegistry<T>& put(const PK& key, const T& entity)
    {
      ensureThreadAffinity();
      auto instance = std::make_shared<T>(entity);

      instances_[key] = instance;
      snapshots_[key] = reflection::make_snapshot(*instance);

      return *this;
    }

    InstanceRegistry<T>& emplace(const PK& key, const T& entity)
    {
      ensureThreadAffinity();
      auto instance = std::make_shared<T>(entity);

      const auto [_, inserted] = instances_.try_emplace(key, instance);
      if (inserted) {
        snapshots_.try_emplace(key, reflection::make_snapshot(*instance));
      }

      return *this;
    }

    void remove(const PK& key)
    {
      ensureThreadAffinity();
      instances_.erase(key);
      snapshots_.erase(key);
    }

    [[nodiscard]]
    std::size_t count() const
    {
      ensureThreadAffinity();
      return instances_.size();
    }

    [[nodiscard]]
    bool hasSnapshot(const T& instance) const
    {
      return hasSnapshot(primaryKey_.get(instance));
    }

    [[nodiscard]]
    bool hasSnapshot(const PK& key) const
    {
      ensureThreadAffinity();
      return snapshots_.contains(key);
    }

    [[nodiscard]]
    bool isDirty(const T& instance) const
    {
      return isDirty(primaryKey_.get(instance));
    }

    [[nodiscard]]
    bool isDirty(const PK& key) const
    {
      if (!has(key) || !hasSnapshot(key))
        return false;

      return reflection::is_dirty(*instances_.at(key), snapshots_.at(key));
    }

    [[nodiscard]]
    bool isDirty(const PK& key, const T& instance) const
    {
      ensureThreadAffinity();
      if (!hasSnapshot(key))
        return false;

      return reflection::is_dirty(instance, snapshots_.at(key));
    }

    [[nodiscard]]
    std::size_t changedFieldCount(const T& instance) const
    {
      return changedFieldCount(primaryKey_.get(instance));
    }

    [[nodiscard]]
    std::size_t changedFieldCount(const PK& key) const
    {
      if (!has(key) || !hasSnapshot(key))
        return 0;

      return reflection::changed_field_count(*instances_.at(key), snapshots_.at(key));
    }

    [[nodiscard]]
    std::size_t changedFieldCount(const PK& key, const T& instance) const
    {
      ensureThreadAffinity();
      if (!hasSnapshot(key))
        return 0;

      return reflection::changed_field_count(instance, snapshots_.at(key));
    }

    template <typename Visitor>
    std::size_t forEachChangedField(const T& instance, Visitor&& visitor) const
    {
      return forEachChangedField(primaryKey_.get(instance), std::forward<Visitor>(visitor));
    }

    template <typename Visitor>
    std::size_t forEachChangedField(const PK& key, Visitor&& visitor) const
    {
      if (!has(key) || !hasSnapshot(key))
        return 0;

      return reflection::for_each_changed_field(
        *instances_.at(key), snapshots_.at(key), std::forward<Visitor>(visitor));
    }

    template <typename Visitor>
    std::size_t forEachChangedField(const PK& key, const T& instance, Visitor&& visitor) const
    {
      ensureThreadAffinity();
      if (!hasSnapshot(key))
        return 0;

      return reflection::for_each_changed_field(instance, snapshots_.at(key), std::forward<Visitor>(visitor));
    }

    void refreshSnapshot(const T& instance)
    {
      refreshSnapshot(primaryKey_.get(instance));
    }

    void refreshSnapshot(const PK& key)
    {
      ensureThreadAffinity();
      if (!has(key))
        return;

      snapshots_[key] = reflection::make_snapshot(*instances_.at(key));
    }

  private:
    void ensureThreadAffinity() const
    {
      if (std::this_thread::get_id() != ownerThread_) {
        throw worm::ConcurrentAccessException(
          "InstanceRegistry accessed from a different thread than it was created on.");
      }
    }

    static constexpr auto primaryKey_ = primary_key_field_of<T>();
    const std::thread::id ownerThread_ = std::this_thread::get_id();
    std::map<PK, std::shared_ptr<T>> instances_;
    std::map<PK, reflection::EntitySnapshot<T>> snapshots_;
  };

  class Registry final
  {
  public:
    Registry() = default;

    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;
    Registry(Registry&&) = delete;
    Registry& operator=(Registry&&) = delete;

    template <PersistableEntity T>
    InstanceRegistry<T>& instances()
    {
      ensureThreadAffinity();
      const std::type_index key{typeid(T)};

      if (!registries_.contains(key)) {
        registries_[key] = std::make_shared<InstanceRegistry<T>>();
      }

      return *std::static_pointer_cast<InstanceRegistry<T>>(registries_[key]);
    }

  private:
    void ensureThreadAffinity() const
    {
      if (std::this_thread::get_id() != ownerThread_) {
        throw worm::ConcurrentAccessException("Registry accessed from a different thread than it was created on.");
      }
    }

    const std::thread::id ownerThread_ = std::this_thread::get_id();
    std::unordered_map<std::type_index, std::shared_ptr<void>> registries_;
  };

} // namespace worm::core
