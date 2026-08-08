#pragma once

#include <core/query/expression.hpp>

#include <cstddef>
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

    friend bool operator==(const Statement&, const Statement&) = default;
  };

  struct StatementHash
  {
    [[nodiscard]]
    std::size_t operator()(const Statement& statement) const noexcept;
  };

  [[nodiscard]]
  bool hasFilterWhere(std::string_view sql, std::string_view qualifier);

} // namespace worm::core
