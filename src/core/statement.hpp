#pragma once

#include <core/expression.hpp>

#include <string>
#include <utility>
#include <vector>

namespace worm::core
{

  struct Statement
  {
    std::string sql;
    std::vector<Parameter> parameters;

    Statement() = default;

    Statement(std::string sql, std::vector<Parameter> parameters = {})
      : sql(std::move(sql)),
        parameters(std::move(parameters))
    {}
  };

} // namespace worm::core
