#include <core/filters.hpp>

#include <errors/invalid-arg-exception.hpp>

#include <iterator>
#include <utility>

worm::core::Filters& worm::core::Filters::add(Expression expression)
{
  if (expression.sql.empty())
    throw worm::InvalidArgException("Where expression cannot be empty.");

  parameters_.insert(parameters_.end(),
                     std::make_move_iterator(expression.parameters.begin()),
                     std::make_move_iterator(expression.parameters.end()));
  expressions_.push_back(std::move(expression));
  return *this;
}

bool worm::core::Filters::empty() const noexcept
{
  return expressions_.empty();
}

std::string worm::core::Filters::sql() const
{
  if (expressions_.empty())
    return {};

  std::string result = "WHERE ";
  for (std::size_t index = 0; index < expressions_.size(); ++index) {
    if (index != 0)
      result += " AND ";
    result += expressions_[index].sql;
  }

  return result;
}

const std::vector<worm::core::Parameter>& worm::core::Filters::parameters() const noexcept
{
  return parameters_;
}
