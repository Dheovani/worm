#include <core/filter.hpp>

#include <utility>

namespace worm::core
{

  Filter::Filter(Expression condition) noexcept
    : condition_(std::move(condition))
  {}

  const Expression& Filter::expression() const noexcept
  {
    return condition_;
  }

  Filter& Filter::andFilter(const Expression& condition)
  {
    condition_ = Predicate::all(condition_, condition);
    return *this;
  }

  Filter& Filter::orFilter(const Expression& condition)
  {
    condition_ = Predicate::any(condition_, condition);
    return *this;
  }

} // namespace worm::core
