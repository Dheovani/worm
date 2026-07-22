#pragma once

#include <optional>
#include <string>
#include <vector>

#include <core/filter.hpp>
#include <core/ordering.hpp>
#include <core/source.hpp>
#include <core/sql-builder.hpp>

namespace worm::core
{

  template <SqlBuilderI SqlBuilderType>
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
      const std::vector<Relation>& relations = {},
      const std::optional<Filter>& filter = std::nullopt,
      const std::vector<Ordering>& ordering = {}) const
    {
      return sqlBuilder_.select(fields, source, relations, filter, ordering);
    }

  private:
    const SqlBuilderType& sqlBuilder_;
  };

} // namespace worm::core
