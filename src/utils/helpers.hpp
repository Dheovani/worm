#pragma once

#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>

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
