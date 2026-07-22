#pragma once

#include <core/expression.hpp>
#include <core/predicate.hpp>

namespace worm::core
{

  class Filter final
  {
  public:
    explicit Filter(Expression condition) noexcept;

    [[nodiscard]]
    const Expression& expression() const noexcept;

    Filter& andFilter(const Expression& condition);

    Filter& orFilter(const Expression& condition);

  private:
    Expression condition_;
  };

} // namespace worm::core
