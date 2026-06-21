#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <core/parameter.hpp>

namespace worm::core
{
	enum class Comparison
	{
		Equal,
		NotEqual,
		Greater,
		GreaterOrEqual,
		Less,
		LessOrEqual,
		Like
	};

	struct Expression
	{
		std::string sql;
		std::vector<Parameter> parameters;
	};

	class Expressions final
	{
	public:
		[[nodiscard]] static Expression Compare(
			std::string_view column,
			Comparison comparison,
			Parameter value
		);

		[[nodiscard]] static Expression IsNull(std::string_view column);
		[[nodiscard]] static Expression IsNotNull(std::string_view column);

		[[nodiscard]] static Expression Between(
			std::string_view column,
			Parameter lower,
			Parameter upper
		);

		[[nodiscard]] static Expression In(
			std::string_view column,
			std::vector<Parameter> values
		);

		[[nodiscard]] static Expression NotIn(
			std::string_view column,
			std::vector<Parameter> values
		);
	};
}
