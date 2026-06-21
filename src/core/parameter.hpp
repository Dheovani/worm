#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <variant>

namespace worm::core
{
	using Parameter = std::variant<
		std::nullptr_t,
		std::int64_t,
		double,
		bool,
		std::string
	>;
}
