#pragma once

#include <concepts>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include <core/model/constraint.hpp>
#include <core/model/schema.hpp>
#include <reflection/concepts.hpp>
#include <reflection/snapshot.hpp>

namespace worm::core
{
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

    template <typename T>
    consteval bool hasConstexprPrimaryKey()
    {
      static_cast<void>(std::remove_cvref_t<T>::primaryKey());
      return true;
    }

    template <typename T>
    consteval bool hasConstexprView()
    {
      static_cast<void>(std::remove_cvref_t<T>::view());
      return true;
    }
  } // namespace detail

  template <typename T>
  concept Entity =
    reflection::Reflectable<std::remove_cvref_t<T>> &&
    reflection::Snapshotable<std::remove_cvref_t<T>> &&
    requires {
      { std::remove_cvref_t<T>::table() } -> std::same_as<Table>;
      { std::remove_cvref_t<T>::primaryKey() } -> std::same_as<PrimaryKey>;
      requires detail::hasConstexprTable<std::remove_cvref_t<T>>();
      requires detail::hasConstexprPrimaryKey<std::remove_cvref_t<T>>();
    };

  template <typename T>
  concept Viewable = reflection::Reflectable<std::remove_cvref_t<T>> && requires {
    { std::remove_cvref_t<T>::view() } -> std::same_as<View>;
    requires detail::hasConstexprView<std::remove_cvref_t<T>>();
  };
} // namespace worm::core
