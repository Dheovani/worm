#pragma once

#include <string>
#include <vector>

#include <core/expression.hpp>

namespace worm::core
{
  class Filters
  {
  public:
    Filters& add(Expression expression);

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::string sql() const;
    [[nodiscard]] const std::vector<Parameter>& parameters() const noexcept;

  private:
    std::vector<Expression> expressions_;
    std::vector<Parameter> parameters_;
  };
} // namespace worm::core
