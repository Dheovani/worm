#pragma once

#include <core/query/expression.hpp>

#include <string>
#include <string_view>
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

    static Statement prepare(std::string sql, std::vector<Parameter> parameters = {})
    {
      return {std::move(sql), std::move(parameters)};
    }
  };

  [[nodiscard]]
  bool hasFilterWhere(std::string_view sql, std::string_view qualifier);

} // namespace worm::core
