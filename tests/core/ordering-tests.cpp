#include <core/ordering.hpp>

#include <errors/invalid-arg-exception.hpp>

#include <iostream>

int main()
{
    using worm::core::Direction;
    using worm::core::OrderBy;
    using worm::core::OrderClause;

    const OrderBy order{"name", Direction::Descending};
    if (order.column() != "name" || order.direction() != Direction::Descending
        || order.sql() != "name DESC") {
        std::cerr << "OrderBy did not preserve its state.\n";
        return 1;
    }

    OrderClause clause;
    if (!clause.empty() || !clause.sql().empty()) {
        std::cerr << "A new OrderClause is not empty.\n";
        return 1;
    }

    clause.add(OrderBy{"name"}).add(OrderBy{"id", Direction::Descending});
    if (clause.empty() || clause.sql() != "ORDER BY name ASC, id DESC") {
        std::cerr << "OrderClause composition is invalid.\n";
        return 1;
    }

    try {
        OrderBy invalid{""};
        static_cast<void>(invalid);
        std::cerr << "OrderBy accepted an empty column.\n";
        return 1;
    }
    catch (const worm::InvalidArgException&) {
    }

    return 0;
}
