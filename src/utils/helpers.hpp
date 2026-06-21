#pragma once

#include <string>
#include <string_view>
#include <cstddef>
#include <type_traits>
#include <unordered_map>
#include <variant>

namespace utils
{
	template <typename _Ty>
	struct is_string_impl : std::false_type {};

	template <>
	struct is_string_impl<std::string> : std::true_type {};

	template <>
	struct is_string_impl<std::string_view> : std::true_type {};

	template <>
	struct is_string_impl<const char*> : std::true_type {};

	template <>
	struct is_string_impl<char*> : std::true_type {};

	template <typename _Ty>
	constexpr bool is_string = is_string_impl<std::decay_t<_Ty>>::value;

	// Checks if a given <typename> is an instance or extends a diferent <typename>
	template <typename _Base, typename _Derived>
	constexpr bool instance_of = std::is_base_of_v<_Base, std::remove_pointer_t<_Derived>>;

	// Checks for functions
	template <typename _Ty>
	constexpr bool is_function = std::is_member_function_pointer_v<_Ty> || std::is_function_v<std::remove_pointer_t<_Ty>>;

	// Checks for attributes
	template <typename _Ty>
	constexpr bool is_attribute = !is_function<_Ty> &&
		(std::is_member_object_pointer_v<_Ty> || std::is_object_v<std::remove_pointer_t<_Ty>>);

	// Returns the index of a given variant option.
	// If the variant is not found, returns std::variant_npos
	template <typename _Ty, typename _Var, size_t _Idx = 0>
	constexpr size_t GetVariantIndex()
	{
		if constexpr (_Idx >= std::variant_size_v<_Var>)
			return std::variant_npos;
		else if constexpr (std::is_same_v<std::variant_alternative_t<_Idx, _Var>, _Ty>)
			return _Idx;
		else
			return GetVariantIndex<_Ty, _Var, _Idx + 1>();
	}

	template <typename _Ty, typename _Var, size_t _Idx = -1>
	constexpr size_t get_variant_index = GetVariantIndex<_Ty, _Var, _Idx + 1>();

	// Checks if a given variant type holds a <typename> option
	template <typename _Ty, typename _Var>
	constexpr bool holds_variant_option = get_variant_index<_Ty, _Var> != std::variant_npos;

	template <typename _Ty, class _Class>
	struct remove_class_pointer {
		using type = _Ty;
	};

	template <typename _Ty, class _Class>
	struct remove_class_pointer<_Ty _Class::*, _Class> {
		using type = _Ty;
	};

	// Specialization to remove a class pointers
	template <typename _Ty, class _Class>
	using remove_class_pointer_t = typename remove_class_pointer<_Ty, _Class>::type;

namespace env
{
	// This function searchs for the .env file in the project's root dir
	const std::string FindEnvInProjectRoot();

	// Loads the .env file from a given path
	const std::unordered_map<std::string, std::string> LoadFromPath(const std::string& path);

	// Returns the database type from the .env file
	const std::string GetDatabaseType();
}
}
