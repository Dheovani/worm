#include <core/expression.hpp>

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
  using worm::core::ExpressionBuilder;

  const auto comparison = ExpressionBuilder::compare("age", Comparison::GreaterOrEqual, std::int64_t{18});
  if (comparison.sql != "age >= ?" || comparison.parameters.size() != 1 ||
      std::get<std::int64_t>(comparison.parameters.front()) != 18) {
    std::cerr << "Comparison expression is invalid.\n";
    return 1;
  }

  const std::vector<std::pair<Comparison, std::string>> operators{
      {Comparison::Equal, "="},           {Comparison::NotEqual, "<>"}, {Comparison::Greater, ">"},
      {Comparison::GreaterOrEqual, ">="}, {Comparison::Less, "<"},      {Comparison::LessOrEqual, "<="},
      {Comparison::Like, "LIKE"}};
  for (const auto& [operation, sqlOperator] : operators) {
    if (ExpressionBuilder::compare("value", operation, true).sql != "value " + sqlOperator + " ?") {
      std::cerr << "A comparison operator was mapped incorrectly.\n";
      return 1;
    }
  }

  const auto isNull = ExpressionBuilder::isNull("deleted_at");
  const auto isNotNull = ExpressionBuilder::isNotNull("created_at");
  const auto between = ExpressionBuilder::between("age", std::int64_t{18}, std::int64_t{65});
  const auto in = ExpressionBuilder::in("id", {std::int64_t{1}, std::int64_t{2}, std::int64_t{3}});
  const auto notIn = ExpressionBuilder::notIn("status", {std::string{"deleted"}, std::string{"blocked"}});

  if (isNull.sql != "deleted_at IS NULL" || !isNull.parameters.empty() || isNotNull.sql != "created_at IS NOT NULL" ||
      !isNotNull.parameters.empty() || between.sql != "age BETWEEN ? AND ?" || between.parameters.size() != 2 ||
      in.sql != "id IN (?, ?, ?)" || in.parameters.size() != 3 || notIn.sql != "status NOT IN (?, ?)" ||
      notIn.parameters.size() != 2) {
    std::cerr << "A compound expression is invalid.\n";
    return 1;
  }

  auto left = ExpressionBuilder::compare("age", Comparison::GreaterOrEqual, std::int64_t{18});
  auto right = ExpressionBuilder::compare("active", Comparison::Equal, true);
  const auto conjunction = ExpressionBuilder::_and(left, right);
  const auto disjunction = ExpressionBuilder::_or(left, right);

  if (conjunction.sql != "age >= ? and active = ?" || disjunction.sql != "age >= ? or active = ?" ||
      conjunction.parameters.size() != 2 || disjunction.parameters.size() != 2) {
    std::cerr << "Expression logical composition is invalid.\n";
    return 1;
  }

  if (!ThrowsInvalidArgument([&] { static_cast<void>(ExpressionBuilder::compare("", Comparison::Equal, true)); }) ||
      !ThrowsInvalidArgument([&] { static_cast<void>(ExpressionBuilder::in("id", {})); }) ||
      !ThrowsInvalidArgument([&] { static_cast<void>(ExpressionBuilder::notIn("id", {})); })) {
    std::cerr << "Expression accepted invalid input.\n";
    return 1;
  }

  return 0;
}
