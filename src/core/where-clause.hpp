#pragma once

#include <string>
#include <vector>

#include <core/expression.hpp>

namespace worm
{
  namespace core
  {
    class WhereClause
    {
    public:
      WhereClause& add(Expression expression);

      [[nodiscard]] bool empty() const noexcept;
      [[nodiscard]] std::string sql() const;
      [[nodiscard]] const std::vector<Parameter>& parameters() const noexcept;

    private:
      std::vector<Expression> expressions_;
      std::vector<Parameter> parameters_;
    };
  } // namespace core
} // namespace worm
