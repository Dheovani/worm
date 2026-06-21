#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <variant>

namespace worm
{
  namespace utils
  {
    template <typename Type> struct is_string_impl : std::false_type
    {};

    template <> struct is_string_impl<std::string> : std::true_type
    {};

    template <> struct is_string_impl<std::string_view> : std::true_type
    {};

    template <> struct is_string_impl<const char*> : std::true_type
    {};

    template <> struct is_string_impl<char*> : std::true_type
    {};

    template <typename Type>
    inline constexpr bool is_string = is_string_impl<std::decay_t<Type>>::value;

    template <typename Base, typename Derived>
    inline constexpr bool instance_of = std::is_base_of_v<Base, std::remove_pointer_t<Derived>>;

    template <typename Type>
    inline constexpr bool is_function =
        std::is_member_function_pointer_v<Type> || std::is_function_v<std::remove_pointer_t<Type>>;

    template <typename Type>
    inline constexpr bool is_attribute =
        !is_function<Type> &&
        (std::is_member_object_pointer_v<Type> || std::is_object_v<std::remove_pointer_t<Type>>);

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
    inline constexpr std::size_t get_variant_index =
        detail::get_variant_index_impl<Type, Variant>();

    template <typename Type, typename Variant>
    inline constexpr bool holds_variant_option =
        get_variant_index<Type, Variant> != std::variant_npos;

    template <typename Type, typename Class> struct remove_class_pointer
    {
      using type = Type;
    };

    template <typename Type, typename Class> struct remove_class_pointer<Type Class::*, Class>
    {
      using type = Type;
    };

    template <typename Type, typename Class>
    using remove_class_pointer_t = typename remove_class_pointer<Type, Class>::type;

    namespace env
    {
      [[nodiscard]] std::string findInProjectRoot();
      [[nodiscard]] std::unordered_map<std::string, std::string>
      loadFromPath(const std::string& path);
      [[nodiscard]] std::string getDatabaseType();
    } // namespace env
  } // namespace utils
} // namespace worm
