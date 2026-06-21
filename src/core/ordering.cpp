#include <core/ordering.hpp>

#include <errors/invalid-arg-exception.hpp>

#include <utility>

worm::core::OrderBy::OrderBy(std::string column, Direction direction)
	: column_(std::move(column)), direction_(direction)
{
	if (column_.empty())
		throw worm::InvalidArgException("Order column cannot be empty.");
}

const std::string& worm::core::OrderBy::column() const noexcept
{
	return column_;
}

worm::core::Direction worm::core::OrderBy::direction() const noexcept
{
	return direction_;
}

std::string worm::core::OrderBy::sql() const
{
	return column_ + (direction_ == Direction::Ascending ? " ASC" : " DESC");
}

worm::core::OrderClause& worm::core::OrderClause::add(OrderBy ordering)
{
	items_.push_back(std::move(ordering));
	return *this;
}

bool worm::core::OrderClause::empty() const noexcept
{
	return items_.empty();
}

std::string worm::core::OrderClause::sql() const
{
	if (items_.empty())
		return {};

	std::string result = "ORDER BY ";
	for (std::size_t index = 0; index < items_.size(); ++index) {
		if (index != 0)
			result += ", ";
		result += items_[index].sql();
	}

	return result;
}
