#pragma once

#include <string_view>
#include <vector>

#include <core/source.hpp>
#include <core/sql-builder.hpp>

namespace worm::core
{

  template <SqlBuilder __SqlBuilder> class QueryBuilder
  {
  public:
    QueryBuilder(const __SqlBuilder& sql_builder) noexcept
      : sqlQuilder_(sql_builder)
    {}

    [[nodiscard]]
    const std::string_view select(
      const std::vector<worm::core::Field>& fields,
      const Source& source,
      const std::vector<Relation>& relations) const noexcept
    {
      return sqlQuilder_.build(fields, source, relations);
    }

  private:
    const __SqlBuilder& sqlQuilder_;
  };

} // namespace worm::core
