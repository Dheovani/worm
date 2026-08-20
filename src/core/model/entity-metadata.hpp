#pragma once

#include <core/model/entity.hpp>
#include <reflection/visit.hpp>

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace worm::core
{
  namespace detail
  {
    struct PrimaryKeyFieldSelector
    {
      template <typename T, typename Field>
      static constexpr bool matches(Field field)
      {
        if (!field.isPersistent()) {
          return false;
        }

        constexpr auto primaryKey = std::remove_cvref_t<T>::primaryKey();
        for (const auto& column : primaryKey.columns()) {
          if (column.columnName == field.columnName()) {
            return true;
          }
        }

        return false;
      }
    };

    struct PersistentFieldSelector
    {
      template <typename T, typename Field>
      static constexpr bool matches(Field field)
      {
        return field.isPersistent();
      }
    };

    template <typename Selector, typename T, std::size_t Index>
    constexpr auto selected_field_tuple()
    {
      constexpr auto fields = std::remove_cvref_t<T>::reflect();
      constexpr auto field = std::get<Index>(fields);

      if constexpr (Selector::template matches<T>(field)) {
        return std::tuple{field};
      } else {
        return std::tuple{};
      }
    }

    template <typename Selector, typename T, std::size_t... Index>
    constexpr auto selected_fields_impl(std::index_sequence<Index...>)
    {
      return std::tuple_cat(selected_field_tuple<Selector, T, Index>()...);
    }

    template <typename Selector, typename T>
    constexpr auto selected_fields_of()
    {
      using EntityType = std::remove_cvref_t<T>;
      using Fields = std::remove_cvref_t<decltype(EntityType::reflect())>;

      return selected_fields_impl<Selector, EntityType>(std::make_index_sequence<std::tuple_size_v<Fields>>{});
    }
  } // namespace detail

  template <Entity T>
  [[nodiscard]]
  constexpr Table table_of()
  {
    return std::remove_cvref_t<T>::table();
  }

  template <Entity T>
  [[nodiscard]]
  constexpr bool has_valid_table()
  {
    return !table_of<T>().empty();
  }

  template <Entity T>
  [[nodiscard]]
  constexpr auto fields_of()
  {
    return std::remove_cvref_t<T>::reflect();
  }

  template <Entity T>
  [[nodiscard]]
  constexpr auto persistent_fields_of()
  {
    return detail::selected_fields_of<detail::PersistentFieldSelector, T>();
  }

  template <Entity T>
  [[nodiscard]]
  constexpr auto primary_key_fields_of()
  {
    return detail::selected_fields_of<detail::PrimaryKeyFieldSelector, T>();
  }

  template <Entity T>
  inline constexpr std::size_t persistent_field_count =
    std::tuple_size_v<std::remove_cvref_t<decltype(persistent_fields_of<T>())>>;

  template <Entity T>
  inline constexpr std::size_t primary_key_count =
    std::tuple_size_v<std::remove_cvref_t<decltype(primary_key_fields_of<T>())>>;

  template <Entity T>
  inline constexpr bool has_single_primary_key = primary_key_count<T> == 1;

  template <Entity T>
  [[nodiscard]]
  constexpr bool has_primary_key()
  {
    return primary_key_count<T> > 0;
  }

  template <Entity T>
  [[nodiscard]]
  constexpr bool is_valid_entity()
  {
    return has_valid_table<T>() && persistent_field_count<T> > 0 && has_single_primary_key<T>;
  }

  template <typename T>
  concept PersistableEntity = Entity<T> && is_valid_entity<std::remove_cvref_t<T>>();

  template <PersistableEntity T>
  [[nodiscard]]
  constexpr auto primary_key_field_of()
  {
    return std::get<0>(primary_key_fields_of<T>());
  }

  template <Viewable T>
  [[nodiscard]]
  constexpr View view_of()
  {
    return std::remove_cvref_t<T>::view();
  }

  template <Viewable T>
  [[nodiscard]]
  constexpr bool has_valid_view()
  {
    return !view_of<T>().empty();
  }

  template <Viewable T>
  [[nodiscard]]
  constexpr auto fields_of()
  {
    return std::remove_cvref_t<T>::reflect();
  }

  template <Viewable T>
  inline constexpr std::size_t view_field_count = reflection::field_count<T>;

  template <Viewable T>
  [[nodiscard]]
  constexpr bool is_valid_view()
  {
    return has_valid_view<T>() && view_field_count<T> > 0;
  }

  template <typename T>
  concept QueryableView = Viewable<T> && is_valid_view<std::remove_cvref_t<T>>();

  template <typename T>
  concept Model = PersistableEntity<T> || QueryableView<T>;
} // namespace worm::core
