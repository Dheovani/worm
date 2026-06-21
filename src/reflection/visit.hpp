#pragma once

#include <cstddef>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

#include <reflection/concepts.hpp>

namespace worm
{
  namespace reflection
  {
    template <Reflectable T>
    inline constexpr std::size_t field_count =
        std::tuple_size_v<std::remove_cvref_t<decltype(std::remove_cvref_t<T>::reflect())>>;

    template <Reflectable T, typename Visitor>
    constexpr void for_each_field(T&& object, Visitor&& visitor)
    {
      const auto fields = std::remove_cvref_t<T>::reflect();

      std::apply(
          [&object, &visitor](const auto&... field) {
            (std::invoke(visitor, field, field.get(object)), ...);
          },
          fields);
    }
  } // namespace reflection
} // namespace worm
