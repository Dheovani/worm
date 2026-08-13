#pragma once

#include <core/query/clauses.hpp>
#include <core/query/filter.hpp>
#include <core/query/pagination.hpp>

#include <optional>
#include <vector>

namespace worm::core
{

  class Criteria final
  {
  public:
    [[nodiscard]]
    const std::vector<Relation>& relations() const noexcept;

    [[nodiscard]]
    const std::optional<Filter>& filter() const noexcept;

    [[nodiscard]]
    const std::vector<Ordering>& ordering() const noexcept;

    [[nodiscard]]
    const std::optional<Pagination>& pagination() const noexcept;

    [[nodiscard]]
    const std::vector<Grouping>& grouping() const noexcept;

    [[nodiscard]]
    const std::optional<Filter>& having() const noexcept;

    Criteria& addRelation(Relation relation);

    Criteria& where(Filter filter);

    Criteria& orderBy(Ordering ordering);

    Criteria& paginate(Pagination pagination);

    Criteria& groupBy(Grouping grouping);

    Criteria& having(Filter filter);

  private:
    std::vector<Relation> relations_;
    std::optional<Filter> filter_;
    std::vector<Ordering> ordering_;
    std::optional<Pagination> pagination_;
    std::vector<Grouping> grouping_;
    std::optional<Filter> having_;
  };

} // namespace worm::core
