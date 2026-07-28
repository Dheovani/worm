#pragma once

#include <concepts>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include <reflection/concepts.hpp>
#include <reflection/snapshot.hpp>

namespace worm::core
{
  class Table
  {
  public:
    explicit constexpr Table(std::string_view name) noexcept
      : name_(name)
    {}

    [[nodiscard]]
    constexpr std::string_view name() const noexcept
    {
      return name_;
    }

    [[nodiscard]]
    constexpr bool empty() const noexcept
    {
      return name_.empty();
    }

    [[nodiscard]]
    friend constexpr bool operator==(
      const Table& left,
      const Table& right) noexcept
    {
      return left.name_ == right.name_;
    }

  private:
    const std::string_view name_;
  };

  enum class EntityState
  {
    // The object exists only in user code and is not tracked by the ORM yet.
    Transient,

    // The object has a known database identity and is tracked by the ORM.
    Managed,

    // The object is tracked and scheduled for deletion.
    Removed,

    // The object has a database identity but is not tracked by the current ORM context.
    Detached
  };

  namespace detail
  {

    template <typename T>
    consteval bool hasConstexprTable()
    {
      static_cast<void>(std::remove_cvref_t<T>::table());
      return true;
    }

  } // namespace detail

  template <typename T>
  concept Entity =
    reflection::Reflectable<std::remove_cvref_t<T>> &&
    reflection::Snapshotable<std::remove_cvref_t<T>> &&
    requires {
      { std::remove_cvref_t<T>::table() } -> std::same_as<Table>;
      requires detail::hasConstexprTable<std::remove_cvref_t<T>>();
    };

} // namespace worm::core
