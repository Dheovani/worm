#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace worm::core
{
	enum class Direction
	{
		Ascending,
		Descending
	};

	class OrderBy
	{
	public:
		explicit OrderBy(
			std::string column,
			Direction direction = Direction::Ascending
		);

		[[nodiscard]] const std::string& column() const noexcept;
		[[nodiscard]] Direction direction() const noexcept;
		[[nodiscard]] std::string sql() const;

	private:
		std::string column_;
		Direction direction_;
	};

	class OrderClause
	{
	public:
		OrderClause& add(OrderBy ordering);

		[[nodiscard]] bool empty() const noexcept;
		[[nodiscard]] std::string sql() const;

	private:
		std::vector<OrderBy> items_;
	};
}
