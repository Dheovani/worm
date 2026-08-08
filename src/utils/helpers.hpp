#pragma once

#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

namespace worm::utils
{
  template <typename Type>
  struct is_string_impl : std::false_type
  {};

  template <>
  struct is_string_impl<std::string> : std::true_type
  {};

  template <>
  struct is_string_impl<std::string_view> : std::true_type
  {};

  template <>
  struct is_string_impl<const char*> : std::true_type
  {};

  template <>
  struct is_string_impl<char*> : std::true_type
  {};

  template <typename Type>
  inline constexpr bool is_string_like = is_string_impl<std::decay_t<Type>>::value;

  template <typename T, typename U = T>
  struct is_equality_comparable : std::false_type
  {};

  template <typename T, typename U>
    requires requires(const std::remove_cvref_t<T>& left, const std::remove_cvref_t<U>& right) {
      { left == right } -> std::convertible_to<bool>;
      { right == left } -> std::convertible_to<bool>;
    }
  struct is_equality_comparable<T, U> : std::true_type
  {};

  template <typename T, typename U = T>
  inline constexpr bool is_equality_comparable_v =
    is_equality_comparable<std::remove_cvref_t<T>, std::remove_cvref_t<U>>::value;

  template <typename T>
  struct is_optional : std::false_type
  {};

  template <typename T>
  struct is_optional<std::optional<T>> : std::true_type
  {};

  template <typename T>
  inline constexpr bool is_optional_v = is_optional<std::remove_cvref_t<T>>::value;

  template <typename T>
  using remove_optional_t = typename std::remove_cvref_t<T>::value_type;

  template <typename T>
  inline constexpr bool is_date_type =
    std::is_same_v<std::remove_cvref_t<T>, std::chrono::time_point<std::chrono::system_clock, std::chrono::days>>;

  template <typename Base, typename Derived>
  inline constexpr bool instance_of = std::is_base_of_v<Base, std::remove_pointer_t<Derived>>;

  template <typename Type>
  inline constexpr bool is_function =
    std::is_member_function_pointer_v<Type> || std::is_function_v<std::remove_pointer_t<Type>>;

  template <typename Type>
  inline constexpr bool is_attribute =
    !is_function<Type> && (std::is_member_object_pointer_v<Type> || std::is_object_v<std::remove_pointer_t<Type>>);

  namespace detail
  {
    template <typename Type, typename Variant, std::size_t Index = 0>
    consteval std::size_t get_variant_index_impl()
    {
      if constexpr (Index >= std::variant_size_v<Variant>)
        return std::variant_npos;
      else if constexpr (std::is_same_v<std::variant_alternative_t<Index, Variant>, Type>)
        return Index;
      else
        return get_variant_index_impl<Type, Variant, Index + 1>();
    }
  } // namespace detail

  template <typename Type, typename Variant>
  inline constexpr std::size_t get_variant_index = detail::get_variant_index_impl<Type, Variant>();

  template <typename Type, typename Variant>
  inline constexpr bool holds_variant_option = get_variant_index<Type, Variant> != std::variant_npos;

  template <typename Type, typename Class>
  struct remove_class_pointer
  {
    using type = Type;
  };

  template <typename Type, typename Class>
  struct remove_class_pointer<Type Class::*, Class>
  {
    using type = Type;
  };

  template <typename Type, typename Class>
  using remove_class_pointer_t = typename remove_class_pointer<Type, Class>::type;

  namespace env
  {
    [[nodiscard]]
    inline std::string envValue(const char* key)
    {
      if (const char* value = std::getenv(key)) {
        return value;
      }

      return {};
    }
  } // namespace env

  namespace strings
  {
    inline bool replaceFirst(std::string& buffer, std::string_view target, std::string_view value)
    {
      const std::size_t pos = buffer.find(target);

      if (pos == std::string::npos) {
        return false;
      }

      buffer.replace(pos, target.size(), value);
      return true;
    }
  } // namespace strings
} // namespace worm::utils
