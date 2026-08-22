#include <core/model/schema-snapshot.hpp>

#include <iostream>

int main()
{
  using worm::core::ColumnTypeKind;

  const worm::core::SchemaTableSnapshot users{
    .schema = "public",
    .name = "users",
    .columns = {{.name = "id", .nullable = false, .generated = true, .unique = true}},
    .primaryKey = {"id"},
  };
  const worm::core::SchemaSnapshot schema{{users}};

  if (users.findColumn("id") == nullptr || users.findColumn("missing") != nullptr ||
      schema.findTable("public", "users") == nullptr || schema.findTable("public", "missing") != nullptr ||
      worm::core::columnTypeKindName(ColumnTypeKind::Boolean) != "boolean" ||
      worm::core::columnTypeKindName(ColumnTypeKind::Decimal) != "decimal" ||
      worm::core::columnTypeKindName(ColumnTypeKind::DateTime) != "datetime" ||
      worm::core::columnTypeKindName(ColumnTypeKind::Unknown) != "unknown" ||
      worm::core::parseColumnTypeKind("int64") != ColumnTypeKind::Int64 ||
      worm::core::parseColumnTypeKind("not-a-type").has_value()) {
    std::cerr << "SchemaSnapshot lookup failed.\n";
    return 1;
  }

  return 0;
}
