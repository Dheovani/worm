#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include <reflection/concepts.hpp>

namespace worm
{
  namespace reflection
  {
    struct FieldNameHasher
    {
      [[nodiscard]] constexpr std::uint64_t operator()(std::string_view value) const noexcept
      {
        std::uint64_t hash = 14695981039346656037ull;
        for (const char character : value) {
          hash ^= static_cast<unsigned char>(character);
          hash *= 1099511628211ull;
        }
        return hash;
      }
    };

    namespace detail
    {
      template <Reflectable T, typename NameSelector, typename Hasher>
      [[nodiscard]] constexpr std::optional<std::size_t>
      find_field_index_impl(std::string_view name, NameSelector&& selectName, Hasher hasher)
      {
        const auto fields = std::remove_cvref_t<T>::reflect();
        const auto requestedHash = std::invoke(hasher, name);
        std::optional<std::size_t> result;
        std::size_t index = 0;

        std::apply(
            [&](const auto&... descriptor) {
              (
                  [&] {
                    const std::string_view candidate = std::invoke(selectName, descriptor);
                    if (!result && std::invoke(hasher, candidate) == requestedHash &&
                        candidate == name) {
                      result = index;
                    }
                    ++index;
                  }(),
                  ...);
            },
            fields);

        return result;
      }

      template <Reflectable T, typename NameSelector, typename Visitor, typename Hasher>
      constexpr bool visit_field_descriptor_impl(std::string_view name, NameSelector&& selectName,
                                                 Visitor&& visitor, Hasher hasher)
      {
        const auto fields = std::remove_cvref_t<T>::reflect();
        const auto requestedHash = std::invoke(hasher, name);
        bool visited = false;

        std::apply(
            [&](const auto&... descriptor) {
              (
                  [&] {
                    const std::string_view candidate = std::invoke(selectName, descriptor);
                    if (!visited && std::invoke(hasher, candidate) == requestedHash &&
                        candidate == name) {
                      std::invoke(visitor, descriptor);
                      visited = true;
                    }
                  }(),
                  ...);
            },
            fields);

        return visited;
      }
    } // namespace detail

    template <Reflectable T, typename Hasher = FieldNameHasher>
    [[nodiscard]] constexpr std::optional<std::size_t> find_field_index(std::string_view name,
                                                                        Hasher hasher = {})
    {
      return detail::find_field_index_impl<T>(
          name, [](const auto& descriptor) { return descriptor.name(); }, hasher);
    }

    template <Reflectable T, typename Hasher = FieldNameHasher>
    [[nodiscard]] constexpr std::optional<std::size_t> find_column_index(std::string_view name,
                                                                         Hasher hasher = {})
    {
      return detail::find_field_index_impl<T>(
          name, [](const auto& descriptor) { return descriptor.columnName(); }, hasher);
    }

    template <Reflectable T, typename Visitor, typename Hasher = FieldNameHasher>
    constexpr bool visit_field_descriptor(std::string_view name, Visitor&& visitor,
                                          Hasher hasher = {})
    {
      return detail::visit_field_descriptor_impl<T>(
          name, [](const auto& descriptor) { return descriptor.name(); },
          std::forward<Visitor>(visitor), hasher);
    }

    template <Reflectable T, typename Visitor, typename Hasher = FieldNameHasher>
    constexpr bool visit_column_descriptor(std::string_view name, Visitor&& visitor,
                                           Hasher hasher = {})
    {
      return detail::visit_field_descriptor_impl<T>(
          name, [](const auto& descriptor) { return descriptor.columnName(); },
          std::forward<Visitor>(visitor), hasher);
    }

    template <Reflectable T, typename Visitor, typename Hasher = FieldNameHasher>
    constexpr bool visit_field(T&& object, std::string_view name, Visitor&& visitor,
                               Hasher hasher = {})
    {
      return visit_field_descriptor<std::remove_cvref_t<T>>(
          name,
          [&object, &visitor](const auto& descriptor) {
            std::invoke(visitor, descriptor, descriptor.get(object));
          },
          hasher);
    }
  } // namespace reflection
} // namespace worm
