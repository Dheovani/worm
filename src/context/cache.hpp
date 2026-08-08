#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <unordered_map>
#include <utility>

namespace worm::context
{
  template <typename Key, typename Value, typename Hash = std::hash<Key>>
  class Cache
  {
  public:
    using Storage = std::unordered_map<Key, Value, Hash>;

    Cache() = default;

    explicit Cache(Storage items)
      : items_(std::move(items))
    {}

    Cache& add(Key key, Value value)
    {
      items_.insert_or_assign(std::move(key), std::move(value));
      return *this;
    }

    [[nodiscard]]
    std::optional<std::reference_wrapper<const Value>> get(const Key& key) const
    {
      const auto item = items_.find(key);
      if (item == items_.end()) {
        return std::nullopt;
      }

      return std::cref(item->second);
    }

    [[nodiscard]]
    bool contains(const Key& key) const
    {
      return items_.contains(key);
    }

    void clear() noexcept
    {
      items_.clear();
    }

    [[nodiscard]]
    std::size_t size() const noexcept
    {
      return items_.size();
    }

    [[nodiscard]]
    bool empty() const noexcept
    {
      return items_.empty();
    }

  private:
    Storage items_;
  };
} // namespace worm::context
