#pragma once

#include <core/model/entity.hpp>
#include <core/model/entity-metadata.hpp>

#include <map>
#include <memory>
#include <typeindex>
#include <unordered_map>

namespace worm::core
{

  template <PersistableEntity T>
  class InstanceRegistry final
  {
    using PrimaryKeyField = decltype(primary_key_field_of<T>());
    using PK = typename PrimaryKeyField::value_type;

  public:
    explicit InstanceRegistry()
      : instances_()
    {}

    [[nodiscard]]
    bool has(const T& instance) const
    {
      return has(primaryKey_.get(instance));
    }

    [[nodiscard]]
    bool has(const PK& key) const
    {
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
      instances_.insert({key, std::make_shared<T>(entity)});
      return *this;
    }

    InstanceRegistry<T>& put(const PK& key, const T& entity)
    {
      instances_[key] = std::make_shared<T>(entity);
      return *this;
    }

    InstanceRegistry<T>& emplace(const PK& key, const T& entity)
    {
      instances_.try_emplace(key, std::make_shared<T>(entity));
      return *this;
    }

    void remove(const PK& key)
    {
      instances_.erase(key);
    }

    [[nodiscard]]
    std::size_t count() const noexcept
    {
      return instances_.size();
    }

  private:
    static constexpr auto primaryKey_ = primary_key_field_of<T>();
    std::map<PK, std::shared_ptr<T>> instances_;
  };

  class Registry final
  {
  public:
    Registry() = default;

    template <PersistableEntity T>
    InstanceRegistry<T>& instances()
    {
      const std::type_index key{typeid(T)};

      if (!registries_.contains(key)) {
        registries_[key] = std::make_shared<InstanceRegistry<T>>();
      }

      return *std::static_pointer_cast<InstanceRegistry<T>>(registries_[key]);
    }

  private:
    std::unordered_map<std::type_index, std::shared_ptr<void>> registries_;
  };

} // namespace worm::core
