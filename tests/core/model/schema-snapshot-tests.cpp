#include <core/model/schema-snapshot.hpp>

#include <iostream>

int main()
{
  const worm::core::SchemaTableSnapshot users{
    .schema = "public",
    .name = "users",
    .columns = {{.name = "id", .nullable = false, .generated = true, .unique = true}},
    .primaryKey = {"id"},
  };
  const worm::core::SchemaSnapshot schema{{users}};

  if (users.findColumn("id") == nullptr || users.findColumn("missing") != nullptr ||
      schema.findTable("public", "users") == nullptr || schema.findTable("public", "missing") != nullptr) {
    std::cerr << "SchemaSnapshot lookup failed.\n";
    return 1;
  }

  return 0;
}
