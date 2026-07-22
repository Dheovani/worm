#include <core/filter.hpp>

#include <cstdint>
#include <iostream>
#include <string>

int main()
{
  using worm::core::Comparison;
  using worm::core::Filter;
  using worm::core::Predicate;

  Filter filter{Predicate::compare("age", Comparison::GreaterOrEqual, std::int64_t{18})};
  filter.andFilter(Predicate::equal("active", true)).orFilter(Predicate::isNull("deleted_at"));

  const auto& expression = filter.expression();

  if (expression.sql != "age >= ? and active = ? or deleted_at IS NULL") {
    std::cerr << "Filter did not compose its predicate SQL correctly.\n";
    return 1;
  }

  if (expression.parameters.size() != 2 ||
      std::get<std::int64_t>(expression.parameters[0]) != 18 ||
      std::get<bool>(expression.parameters[1]) != true) {
    std::cerr << "Filter did not preserve predicate parameters correctly.\n";
    return 1;
  }

  return 0;
}
