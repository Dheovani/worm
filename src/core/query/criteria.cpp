#include <core/query/criteria.hpp>

#include <utility>

namespace worm::core
{

  const std::vector<Relation>& Criteria::relations() const noexcept
  {
    return relations_;
  }

  const std::optional<Filter>& Criteria::filter() const noexcept
  {
    return filter_;
  }

  const std::vector<Ordering>& Criteria::ordering() const noexcept
  {
    return ordering_;
  }

  const std::optional<Pagination>& Criteria::pagination() const noexcept
  {
    return pagination_;
  }

  const std::vector<Grouping>& Criteria::grouping() const noexcept
  {
    return grouping_;
  }

  const std::optional<Filter>& Criteria::having() const noexcept
  {
    return having_;
  }

  Criteria& Criteria::addRelation(Relation relation)
  {
    relations_.push_back(std::move(relation));
    return *this;
  }

  Criteria& Criteria::where(Filter filter)
  {
    filter_ = std::move(filter);
    return *this;
  }

  Criteria& Criteria::orderBy(Ordering ordering)
  {
    ordering_.push_back(ordering);
    return *this;
  }

  Criteria& Criteria::paginate(Pagination pagination)
  {
    pagination_.emplace(pagination.limit(), pagination.offset());
    return *this;
  }

  Criteria& Criteria::groupBy(Grouping grouping)
  {
    grouping_.push_back(grouping);
    return *this;
  }

  Criteria& Criteria::having(Filter filter)
  {
    having_ = std::move(filter);
    return *this;
  }

} // namespace worm::core
