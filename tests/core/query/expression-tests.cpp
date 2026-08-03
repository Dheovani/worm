#include <core/query/predicate.hpp>

#include <errors/invalid-arg-exception.hpp>

#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace
{
  template <typename Callable>
  bool ThrowsInvalidArgument(Callable&& callable)
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
  using worm::core::Predicate;

  const auto comparison = Predicate::compare("age", Comparison::GreaterOrEqual, std::int64_t{18});
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
    if (Predicate::compare("value", operation, true).sql != "value " + sqlOperator + " ?") {
      std::cerr << "A comparison operator was mapped incorrectly.\n";
      return 1;
    }
  }

  const auto isNull = Predicate::isNull("deleted_at");
  const auto isNotNull = Predicate::isNotNull("created_at");
  const auto between = Predicate::between("age", std::int64_t{18}, std::int64_t{65});
  const auto in = Predicate::in("id", {std::int64_t{1}, std::int64_t{2}, std::int64_t{3}});
  const auto notIn = Predicate::notIn("status", {std::string{"deleted"}, std::string{"blocked"}});

  if (isNull.sql != "deleted_at IS NULL" || !isNull.parameters.empty() || isNotNull.sql != "created_at IS NOT NULL" ||
      !isNotNull.parameters.empty() || between.sql != "age BETWEEN ? AND ?" || between.parameters.size() != 2 ||
      in.sql != "id IN (?, ?, ?)" || in.parameters.size() != 3 || notIn.sql != "status NOT IN (?, ?)" ||
      notIn.parameters.size() != 2) {
    std::cerr << "A compound expression is invalid.\n";
    return 1;
  }

  auto left = Predicate::compare("age", Comparison::GreaterOrEqual, std::int64_t{18});
  auto right = Predicate::equal("active", true);
  const auto conjunction = Predicate::all(left, right);
  const auto disjunction = Predicate::any(left, right);
  const auto negation = Predicate::not_(disjunction);
  const auto third = Predicate::isNull("deleted_at");
  const auto compoundConjunction = Predicate::all({left, right, third});
  const auto compoundDisjunction = Predicate::any({left, right, third});

  if (conjunction.sql != "(age >= ?) and (active = ?)" || disjunction.sql != "(age >= ?) or (active = ?)" ||
      negation.sql != "not ((age >= ?) or (active = ?))" || conjunction.parameters.size() != 2 ||
      disjunction.parameters.size() != 2 || negation.parameters.size() != 2) {
    std::cerr << "Expression logical composition is invalid.\n";
    return 1;
  }

  if (compoundConjunction.sql != "(age >= ?) and (active = ?) and (deleted_at IS NULL)" ||
      compoundDisjunction.sql != "(age >= ?) or (active = ?) or (deleted_at IS NULL)" ||
      compoundConjunction.parameters.size() != 2 || compoundDisjunction.parameters.size() != 2) {
    std::cerr << "Expression variadic logical composition is invalid.\n";
    return 1;
  }

  if (!ThrowsInvalidArgument([&] { static_cast<void>(Predicate::compare("", Comparison::Equal, true)); }) ||
      !ThrowsInvalidArgument([&] { static_cast<void>(Predicate::in("id", {})); }) ||
      !ThrowsInvalidArgument([&] { static_cast<void>(Predicate::notIn("id", {})); }) ||
      !ThrowsInvalidArgument([&] { static_cast<void>(Predicate::all({})); }) ||
      !ThrowsInvalidArgument([&] { static_cast<void>(Predicate::any({})); })) {
    std::cerr << "Expression accepted invalid input.\n";
    return 1;
  }

  return 0;
}
