#pragma once

#include <concepts>
#include <cstddef>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

#include <reflection/concepts.hpp>
#include <reflection/visit.hpp>

namespace worm
{
  namespace reflection
  {
    namespace detail
    {
      template <typename Entity, std::size_t... Indexes>
      [[nodiscard]] constexpr bool has_snapshot_fields(std::index_sequence<Indexes...>)
      {
        using Fields = std::remove_cvref_t<decltype(Entity::reflect())>;
        return (std::copy_constructible<
                    std::remove_cv_t<typename std::tuple_element_t<Indexes, Fields>::value_type>> &&
                ...) &&
               (std::equality_comparable<
                    std::remove_cv_t<typename std::tuple_element_t<Indexes, Fields>::value_type>> &&
                ...);
      }

      template <typename Entity, std::size_t... Indexes>
      [[nodiscard]] constexpr auto snapshot_type(std::index_sequence<Indexes...>)
          -> std::tuple<std::remove_cv_t<typename std::tuple_element_t<
              Indexes, std::remove_cvref_t<decltype(Entity::reflect())>>::value_type>...>;

      template <typename Entity, std::size_t... Indexes>
      [[nodiscard]] constexpr auto make_snapshot_impl(const Entity& object,
                                                      std::index_sequence<Indexes...>)
      {
        const auto fields = Entity::reflect();
        return std::tuple{std::get<Indexes>(fields).get(object)...};
      }

      template <typename Entity, typename Snapshot, typename Visitor, std::size_t... Indexes>
      constexpr std::size_t for_each_changed_field_impl(const Entity& object,
                                                        const Snapshot& snapshot, Visitor&& visitor,
                                                        std::index_sequence<Indexes...>)
      {
        const auto fields = Entity::reflect();
        std::size_t changed = 0;

        (
            [&] {
              const auto& descriptor = std::get<Indexes>(fields);
              const auto& previousValue = std::get<Indexes>(snapshot);
              const auto& currentValue = descriptor.get(object);

              if (!descriptor.isIgnored() && currentValue != previousValue) {
                std::invoke(visitor, descriptor, previousValue, currentValue);
                ++changed;
              }
            }(),
            ...);

        return changed;
      }
    } // namespace detail

    template <typename T>
    concept Snapshotable =
        Reflectable<T> &&
        detail::has_snapshot_fields<std::remove_cvref_t<T>>(
            std::make_index_sequence<std::tuple_size_v<
                std::remove_cvref_t<decltype(std::remove_cvref_t<T>::reflect())>>>{});

    template <Snapshotable T>
    using EntitySnapshot = decltype(detail::snapshot_type<std::remove_cvref_t<T>>(
        std::make_index_sequence<field_count<std::remove_cvref_t<T>>>{}));

    template <Snapshotable T>
    [[nodiscard]] constexpr EntitySnapshot<T> make_snapshot(const T& object)
    {
      using Entity = std::remove_cvref_t<T>;
      return detail::make_snapshot_impl<Entity>(object,
                                                std::make_index_sequence<field_count<Entity>>{});
    }

    template <Snapshotable T, typename Visitor>
    constexpr std::size_t for_each_changed_field(const T& object, const EntitySnapshot<T>& snapshot,
                                                 Visitor&& visitor)
    {
      using Entity = std::remove_cvref_t<T>;
      return detail::for_each_changed_field_impl<Entity>(
          object, snapshot, std::forward<Visitor>(visitor),
          std::make_index_sequence<field_count<Entity>>{});
    }

    template <Snapshotable T>
    [[nodiscard]] constexpr std::size_t changed_field_count(const T& object,
                                                            const EntitySnapshot<T>& snapshot)
    {
      return for_each_changed_field(object, snapshot, [](const auto&, const auto&, const auto&) {});
    }

    template <Snapshotable T>
    [[nodiscard]] constexpr bool is_dirty(const T& object, const EntitySnapshot<T>& snapshot)
    {
      return changed_field_count(object, snapshot) != 0;
    }
  } // namespace reflection
} // namespace worm
