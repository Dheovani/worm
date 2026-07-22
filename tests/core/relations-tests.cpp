// relations-tests.cpp
#include <core/relations.hpp>

#include <errors/invalid-arg-exception.hpp>

#include <cstdint>
#include <iostream>
#include <optional>

int main()
{
  using worm::core::Expression;
  using worm::core::Join;
  using worm::core::Relations;
  using worm::core::Table;

  const Table users{"users"};
  const Table orders{"orders"};
  const Table payments{"payments"};

  Relations relations;
  if (!relations.empty() || !relations.sql().empty()) {
    std::cerr << "A new Relations is not empty.\n";
    return 1;
  }

  relations.from(users, std::nullopt);

  if (relations.empty() || relations.sql() != "FROM users") {
    std::cerr << "Relations generated an invalid single FROM clause.\n";
    return 1;
  }

  Relations aliased;
  aliased.from(users, "u");

  if (aliased.sql() != "FROM users u") {
    std::cerr << "Relations generated an invalid aliased FROM clause.\n";
    return 1;
  }

  Relations multipleSources;
  multipleSources.from(users, "u")
      .from(orders, "o");

  if (multipleSources.sql() != "FROM users u, orders o") {
    std::cerr << "Relations generated an invalid multiple source FROM clause.\n";
    return 1;
  }

  Relations joined;
  joined.from(users, "u")
      .join(Join::Inner,
            orders,
            "o",
            Expression{"u.id = o.user_id", {}})
      .join(Join::Left,
            payments,
            "p",
            Expression{"o.id = p.order_id", {}});

  if (joined.sql() !=
      "FROM users u INNER JOIN orders o ON (u.id = o.user_id)"
      " LEFT JOIN payments p ON (o.id = p.order_id)") {
    std::cerr << "Relations generated an invalid JOIN clause.\n";
    return 1;
  }

  Relations allJoinTypes;
  allJoinTypes.from(users, "u")
      .join(Join::Inner, orders, "oi", Expression{"u.id = oi.user_id", {}})
      .join(Join::Left, orders, "ol", Expression{"u.id = ol.user_id", {}})
      .join(Join::Right, orders, "orx", Expression{"u.id = orx.user_id", {}})
      .join(Join::Full, orders, "ofx", Expression{"u.id = ofx.user_id", {}})
      .join(Join::Cross, orders, "oc", Expression{"1 = 1", {}});

  if (allJoinTypes.sql() !=
      "FROM users u"
      " INNER JOIN orders oi ON (u.id = oi.user_id)"
      " LEFT JOIN orders ol ON (u.id = ol.user_id)"
      " RIGHT JOIN orders orx ON (u.id = orx.user_id)"
      " FULL OUTER JOIN orders ofx ON (u.id = ofx.user_id)"
      " CROSS JOIN orders oc ON (1 = 1)") {
    std::cerr << "Relations generated an invalid JOIN type.\n";
    return 1;
  }

  try {
    joined.join(Join::Inner, orders, "o2", Expression{});
    std::cerr << "Relations accepted an empty join expression.\n";
    return 1;
  } catch (const worm::InvalidArgException&) {}

  return 0;
}