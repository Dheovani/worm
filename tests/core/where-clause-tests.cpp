#include <core/where-clause.hpp>

#include <errors/invalid-arg-exception.hpp>

#include <cstdint>
#include <iostream>

int main()
{
    using worm::core::Comparison;
    using worm::core::Expression;
    using worm::core::Expressions;
    using worm::core::WhereClause;

    WhereClause clause;
    if (!clause.empty() || !clause.sql().empty() || !clause.parameters().empty()) {
        std::cerr << "A new WhereClause is not empty.\n";
        return 1;
    }

    clause
        .add(Expressions::Compare("age", Comparison::GreaterOrEqual, std::int64_t{18}))
        .add(Expressions::In("id", {std::int64_t{7}, std::int64_t{9}}))
        .add(Expressions::IsNull("deleted_at"));

    if (clause.sql() != "WHERE age >= ? AND id IN (?, ?) AND deleted_at IS NULL"
        || clause.parameters().size() != 3
        || std::get<std::int64_t>(clause.parameters()[0]) != 18
        || std::get<std::int64_t>(clause.parameters()[1]) != 7
        || std::get<std::int64_t>(clause.parameters()[2]) != 9) {
        std::cerr << "WhereClause composition or parameter order is invalid.\n";
        return 1;
    }

    try {
        clause.add(Expression{});
        std::cerr << "WhereClause accepted an empty expression.\n";
        return 1;
    }
    catch (const worm::InvalidArgException&) {
    }

    return 0;
}
