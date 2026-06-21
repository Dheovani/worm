#include <core/expression.hpp>

#include <errors/invalid-arg-exception.hpp>

#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace
{
    template<typename Callable>
    bool ThrowsInvalidArgument(Callable&& callable)
    {
        try {
            std::forward<Callable>(callable)();
        }
        catch (const worm::InvalidArgException&) {
            return true;
        }
        return false;
    }
}

int main()
{
    using worm::core::Comparison;
    using worm::core::Expressions;

    const auto comparison = Expressions::Compare(
        "age", Comparison::GreaterOrEqual, std::int64_t{18});
    if (comparison.sql != "age >= ?" || comparison.parameters.size() != 1
        || std::get<std::int64_t>(comparison.parameters.front()) != 18) {
        std::cerr << "Comparison expression is invalid.\n";
        return 1;
    }

    const std::vector<std::pair<Comparison, std::string>> operators{
        {Comparison::Equal, "="}, {Comparison::NotEqual, "<>"},
        {Comparison::Greater, ">"}, {Comparison::GreaterOrEqual, ">="},
        {Comparison::Less, "<"}, {Comparison::LessOrEqual, "<="},
        {Comparison::Like, "LIKE"}
    };
    for (const auto& [operation, sqlOperator] : operators) {
        if (Expressions::Compare("value", operation, true).sql
            != "value " + sqlOperator + " ?") {
            std::cerr << "A comparison operator was mapped incorrectly.\n";
            return 1;
        }
    }

    const auto isNull = Expressions::IsNull("deleted_at");
    const auto isNotNull = Expressions::IsNotNull("created_at");
    const auto between = Expressions::Between("age", std::int64_t{18}, std::int64_t{65});
    const auto in = Expressions::In("id", {std::int64_t{1}, std::int64_t{2}, std::int64_t{3}});
    const auto notIn = Expressions::NotIn(
        "status", {std::string{"deleted"}, std::string{"blocked"}});

    if (isNull.sql != "deleted_at IS NULL" || !isNull.parameters.empty()
        || isNotNull.sql != "created_at IS NOT NULL" || !isNotNull.parameters.empty()
        || between.sql != "age BETWEEN ? AND ?" || between.parameters.size() != 2
        || in.sql != "id IN (?, ?, ?)" || in.parameters.size() != 3
        || notIn.sql != "status NOT IN (?, ?)" || notIn.parameters.size() != 2) {
        std::cerr << "A compound expression is invalid.\n";
        return 1;
    }

    if (!ThrowsInvalidArgument([&] {
            static_cast<void>(Expressions::Compare("", Comparison::Equal, true));
        })
        || !ThrowsInvalidArgument([&] {
            static_cast<void>(Expressions::In("id", {}));
        })
        || !ThrowsInvalidArgument([&] {
            static_cast<void>(Expressions::NotIn("id", {}));
        })) {
        std::cerr << "Expression accepted invalid input.\n";
        return 1;
    }

    return 0;
}
