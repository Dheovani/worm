#include <core/query/clauses.hpp>

#include <iostream>

int main()
{
  using worm::core::OrderDirection;
  using worm::core::Ordering;

  const Ordering defaultOrder{"name"};
  if (defaultOrder.column != "name" || defaultOrder.direction != OrderDirection::Ascending) {
    std::cerr << "Ordering did not preserve its default direction.\n";
    return 1;
  }

  const Ordering descendingOrder{"created_at", OrderDirection::Descending};
  if (descendingOrder.column != "created_at" || descendingOrder.direction != OrderDirection::Descending) {
    std::cerr << "Ordering did not preserve its explicit direction.\n";
    return 1;
  }

  return 0;
}
