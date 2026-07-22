#pragma once

#include <string>
#include <vector>

#include <core/source.hpp>
#include <core/sql-builder.hpp>

namespace worm::core
{

  template <SqlBuilder SqlBuilderType>
  class QueryBuilder
  {
  public:
    explicit QueryBuilder(const SqlBuilderType& sqlBuilder) noexcept
      : sqlBuilder_(sqlBuilder)
    {}

    [[nodiscard]]
    std::string select(
      const std::vector<worm::core::Field>& fields,
      const Source& source,
      const std::vector<Relation>& relations) const
    {
      return sqlBuilder_.select(fields, source, relations);
    }

  private:
    const SqlBuilderType& sqlBuilder_;
  };

} // namespace worm::core
