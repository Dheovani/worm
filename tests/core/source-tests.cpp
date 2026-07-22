#include <core/source.hpp>

#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace
{
  template <typename T, typename Expected> bool holdsValue(const T& value, const Expected& expected)
  {
    return std::holds_alternative<Expected>(value) && std::get<Expected>(value) == expected;
  }
} // namespace

int main()
{
  using worm::core::Expression;
  using worm::core::Field;
  using worm::core::Join;
  using worm::core::Parameter;
  using worm::core::Relation;
  using worm::core::Source;

  const Source users{"users"};
  if (users.name != "users" || users.alias.has_value()) {
    std::cerr << "Source did not preserve an unaliased table name.\n";
    return 1;
  }

  const Source aliasedUsers{"users", "u"};
  if (aliasedUsers.name != "users" || aliasedUsers.alias.value() != "u") {
    std::cerr << "Source did not preserve an aliased table name.\n";
    return 1;
  }

  const Field id{"id", aliasedUsers};
  if (id.name != "id" || id.source.name != "users" || id.source.alias.value() != "u") {
    std::cerr << "Field did not preserve its name and source envelope.\n";
    return 1;
  }

  Expression predicate{"u.id = o.user_id", {std::int64_t{7}, std::string{"paid"}}};
  const Relation relation{Join::Left, aliasedUsers, Source{"orders", "o"}, predicate};

  if (relation.joinType != Join::Left || relation.baseSource.alias.value() != "u" ||
      relation.joinedSource.name != "orders" || relation.condition.sql != "u.id = o.user_id") {
    std::cerr << "Relation did not preserve join/source/expression data.\n";
    return 1;
  }

  if (relation.condition.parameters.size() != 2 || !holdsValue(relation.condition.parameters[0], std::int64_t{7}) ||
      !holdsValue(relation.condition.parameters[1], std::string{"paid"})) {
    std::cerr << "Relation expression parameters were not preserved.\n";
    return 1;
  }

  return 0;
}
