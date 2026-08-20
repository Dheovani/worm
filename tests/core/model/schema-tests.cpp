#include <core/model/schema.hpp>

#include <iostream>

int main()
{
  constexpr worm::core::Schema emptySchema;
  constexpr worm::core::Schema publicSchema{"public"};
  constexpr worm::core::Table users{publicSchema, "users"};
  constexpr worm::core::Table sameUsers{worm::core::Schema{"public"}, "users"};
  constexpr worm::core::Table usersWithoutSchema{"users"};
  constexpr worm::core::Column idColumn{"id", users};
  constexpr worm::core::View activeUsers{publicSchema, "active_users"};
  constexpr worm::core::View definedActiveUsers = activeUsers.definedBy("select * from users where active = true");
  constexpr worm::core::View updatableActiveUsers = definedActiveUsers.asUpdatable();

  static_assert(emptySchema.empty());
  static_assert(publicSchema.name() == "public");
  static_assert(users.schema() == publicSchema);
  static_assert(users.name() == "users");
  static_assert(users == sameUsers);
  static_assert(!(users == usersWithoutSchema));
  static_assert(idColumn.columnName == "id");
  static_assert(idColumn.table() == users);
  static_assert(activeUsers.definition().empty());
  static_assert(!activeUsers.updatable());
  static_assert(definedActiveUsers.definition() == "select * from users where active = true");
  static_assert(!definedActiveUsers.updatable());
  static_assert(updatableActiveUsers.definition() == definedActiveUsers.definition());
  static_assert(updatableActiveUsers.updatable());
  static_assert(activeUsers.definition().empty());
  static_assert(!activeUsers.updatable());

  if (updatableActiveUsers.schema() != publicSchema || updatableActiveUsers.name() != "active_users" ||
      updatableActiveUsers.definition().empty()) {
    std::cerr << "Schema, table, column, or view metadata did not preserve immutable values.\n";
    return 1;
  }

  return 0;
}
