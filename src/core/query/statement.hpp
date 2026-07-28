#pragma once

#include <core/query/expression.hpp>

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

    static Statement from(std::string sql, std::vector<Parameter> parameters = {})
    {
      return {std::move(sql), std::move(parameters)};
    }
  };

} // namespace worm::core
