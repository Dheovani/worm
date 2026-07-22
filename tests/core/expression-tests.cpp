#include <core/sql-builder.hpp>

#include <errors/invalid-arg-exception.hpp>

#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace
{
  template <typename Callable> bool ThrowsInvalidArgument(Callable&& callable)
  {
    try {
      std::forward<Callable>(callable)();
    } catch (const worm::InvalidArgException&) {
      return true;
    }
    return false;
  }
} // namespace

int main()
{
  using worm::core::Comparison;
  using worm::core::PgBuilder;

  const PgBuilder sqlBuilder;

  const auto comparison = sqlBuilder.compare("age", Comparison::GreaterOrEqual, std::int64_t{18});
  if (comparison.sql != "age >= ?" || comparison.parameters.size() != 1 ||
      std::get<std::int64_t>(comparison.parameters.front()) != 18) {
    std::cerr << "Comparison expression is invalid.\n";
    return 1;
  }

  const std::vector<std::pair<Comparison, std::string>> operators{{Comparison::Equal, "="},
                                                                  {Comparison::NotEqual, "<>"},
                                                                  {Comparison::Greater, ">"},
                                                                  {Comparison::GreaterOrEqual, ">="},
                                                                  {Comparison::Less, "<"},
                                                                  {Comparison::LessOrEqual, "<="},
                                                                  {Comparison::Like, "LIKE"}};
  for (const auto& [operation, sqlOperator] : operators) {
    if (sqlBuilder.compare("value", operation, true).sql != "value " + sqlOperator + " ?") {
      std::cerr << "A comparison operator was mapped incorrectly.\n";
      return 1;
    }
  }

  const auto isNull = sqlBuilder.isNull("deleted_at");
  const auto isNotNull = sqlBuilder.isNotNull("created_at");
  const auto between = sqlBuilder.between("age", std::int64_t{18}, std::int64_t{65});
  const auto in = sqlBuilder.in("id", {std::int64_t{1}, std::int64_t{2}, std::int64_t{3}});
  const auto notIn = sqlBuilder.notIn("status", {std::string{"deleted"}, std::string{"blocked"}});

  if (isNull.sql != "deleted_at IS NULL" || !isNull.parameters.empty() || isNotNull.sql != "created_at IS NOT NULL" ||
      !isNotNull.parameters.empty() || between.sql != "age BETWEEN ? AND ?" || between.parameters.size() != 2 ||
      in.sql != "id IN (?, ?, ?)" || in.parameters.size() != 3 || notIn.sql != "status NOT IN (?, ?)" ||
      notIn.parameters.size() != 2) {
    std::cerr << "A compound expression is invalid.\n";
    return 1;
  }

  const auto renderedBetween = sqlBuilder.renderExpression(between);
  const auto renderedIn = sqlBuilder.renderExpression(in, 4);
  if (renderedBetween != "age BETWEEN $1 AND $2" || renderedIn != "id IN ($4, $5, $6)") {
    std::cerr << "PostgreSQL expression rendering is invalid.\n";
    return 1;
  }

  auto left = sqlBuilder.compare("age", Comparison::GreaterOrEqual, std::int64_t{18});
  auto right = sqlBuilder.compare("active", Comparison::Equal, true);
  const auto conjunction = sqlBuilder._and(left, right);
  const auto disjunction = sqlBuilder._or(left, right);

  if (conjunction.sql != "age >= ? and active = ?" || disjunction.sql != "age >= ? or active = ?" ||
      conjunction.parameters.size() != 2 || disjunction.parameters.size() != 2) {
    std::cerr << "Expression logical composition is invalid.\n";
    return 1;
  }

  if (!ThrowsInvalidArgument([&] { static_cast<void>(sqlBuilder.compare("", Comparison::Equal, true)); }) ||
      !ThrowsInvalidArgument([&] { static_cast<void>(sqlBuilder.in("id", {})); }) ||
      !ThrowsInvalidArgument([&] { static_cast<void>(sqlBuilder.notIn("id", {})); })) {
    std::cerr << "Expression accepted invalid input.\n";
    return 1;
  }

  return 0;
}
