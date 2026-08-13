#include <core/query/criteria.hpp>

#include <core/query/predicate.hpp>

#include <cstdint>
#include <iostream>
#include <string>
#include <variant>

int main()
{
  using worm::core::Comparison;
  using worm::core::Criteria;
  using worm::core::Filter;
  using worm::core::Grouping;
  using worm::core::Join;
  using worm::core::OrderDirection;
  using worm::core::Ordering;
  using worm::core::Pagination;
  using worm::core::Predicate;
  using worm::core::Relation;
  using worm::core::Source;

  const Source users{"users", "u"};
  const Source orders{"orders", "o"};

  Criteria criteria;
  criteria.addRelation(Relation{Join::Inner, users, orders, Predicate::equal("u.id", std::int64_t{7})})
    .where(Filter{Predicate::equal("u.active", true)})
    .groupBy(Grouping{"u.id"})
    .having(Filter{Predicate::compare("count(*)", Comparison::Greater, std::int64_t{0})})
    .orderBy(Ordering{"u.name", OrderDirection::Descending})
    .paginate(Pagination{10, 20});

  if (criteria.relations().size() != 1 || criteria.relations()[0].joinedSource.name != "orders") {
    std::cerr << "Criteria did not preserve its relation envelope.\n";
    return 1;
  }

  if (!criteria.filter().has_value() || criteria.filter()->expression().parameters.size() != 1 ||
      !std::holds_alternative<bool>(criteria.filter()->expression().parameters[0])) {
    std::cerr << "Criteria did not preserve its filter envelope.\n";
    return 1;
  }

  if (criteria.grouping().size() != 1 || criteria.grouping()[0].column != "u.id" || !criteria.having().has_value()) {
    std::cerr << "Criteria did not preserve grouping or having.\n";
    return 1;
  }

  if (criteria.ordering().size() != 1 || criteria.ordering()[0].direction != OrderDirection::Descending ||
      !criteria.pagination().has_value() || criteria.pagination()->limit() != 10 ||
      criteria.pagination()->offset() != 20) {
    std::cerr << "Criteria did not preserve ordering or pagination.\n";
    return 1;
  }

  return 0;
}
