#include <core/query/clauses.hpp>

#include <iostream>

int main()
{
  const worm::core::Grouping grouping{"users.id"};

  if (grouping.column != "users.id") {
    std::cerr << "Grouping did not preserve its column.\n";
    return 1;
  }

  return 0;
}
