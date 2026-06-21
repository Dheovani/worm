#pragma once

#include <concepts>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

#include <reflection/field.hpp>

namespace worm
{
  namespace reflection
  {
    namespace detail
    {
      template <typename Type, typename Tuple, std::size_t... Indexes>
      consteval bool has_valid_fields(std::index_sequence<Indexes...>)
      {
        return (is_field_descriptor_v<std::tuple_element_t<Indexes, Tuple>> && ...) &&
               (std::same_as<typename std::tuple_element_t<Indexes, Tuple>::owner_type, Type> &&
                ...);
      }

      template <typename T> consteval bool has_valid_reflection()
      {
        using Type = std::remove_cvref_t<T>;

        if constexpr (!requires { Type::reflect(); }) {
          return false;
        } else {
          using Fields = std::remove_cvref_t<decltype(Type::reflect())>;

          if constexpr (!requires { std::tuple_size<Fields>::value; }) {
            return false;
          } else {
            return has_valid_fields<Type, Fields>(
                std::make_index_sequence<std::tuple_size_v<Fields>>{});
          }
        }
      }
    } // namespace detail

    template <typename T>
    concept Reflectable = detail::has_valid_reflection<T>();
  } // namespace reflection
} // namespace worm
