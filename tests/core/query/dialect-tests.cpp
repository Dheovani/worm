#include <core/query/dialect.hpp>

#include <errors/sql-build-exception.hpp>

#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace
{
  bool hasDialectContract(
    const worm::core::Dialect& dialect,
    std::size_t placeholderIndex,
    const std::string& expectedPlaceholder,
    const std::string& escapedIdentifier,
    const std::string& expectedIdentifier,
    const std::string& expectedEscapedIdentifier)
  {
    if (dialect.placeholder(placeholderIndex) != expectedPlaceholder) {
      std::cerr << "Dialect rendered an unexpected placeholder.\n";
      return false;
    }

    if (dialect.quoteIdentifier("users") != expectedIdentifier) {
      std::cerr << "Dialect rendered an unexpected quoted identifier.\n";
      return false;
    }

    if (dialect.quoteIdentifier(escapedIdentifier) != expectedEscapedIdentifier) {
      std::cerr << "Dialect did not escape identifier delimiters.\n";
      return false;
    }

    return true;
  }
} // namespace

int main()
{
  const worm::core::PostgresDialect postgresDialect;
  const worm::core::MySqlDialect mySqlDialect;
  const worm::core::SqliteDialect sqliteDialect;
  const worm::core::SqlServerDialect sqlServerDialect;

  if (!hasDialectContract(postgresDialect, 3, "$3", "weird\"name", "\"users\"", "\"weird\"\"name\"")) {
    return 1;
  }

  if (!hasDialectContract(mySqlDialect, 3, "?", "weird`name", "`users`", "`weird``name`")) {
    return 1;
  }

  if (!hasDialectContract(sqliteDialect, 3, "?", "weird\"name", "\"users\"", "\"weird\"\"name\"")) {
    return 1;
  }

  if (!hasDialectContract(sqlServerDialect, 3, "?", "weird]name", "[users]", "[weird]]name]")) {
    return 1;
  }

  const worm::core::ColumnType sizedString{
    .kind = worm::core::ColumnTypeKind::String,
    .length = std::size_t{120},
  };
  const worm::core::ColumnType decimal{
    .kind = worm::core::ColumnTypeKind::Decimal,
    .precision = std::size_t{10},
    .scale = std::size_t{2},
  };
  const worm::core::ColumnType zonedDateTime{
    .kind = worm::core::ColumnTypeKind::DateTime,
    .withTimeZone = true,
  };
  const worm::core::ColumnType postgresEnum{
    .kind = worm::core::ColumnTypeKind::Enum,
    .enumeration =
      worm::core::NativeEnum{.schema = "public", .name = "account_status", .values = {"active", "on'hold"}},
  };
  const worm::core::ColumnType mySqlEnum{
    .kind = worm::core::ColumnTypeKind::Enum,
    .enumeration = worm::core::NativeEnum{.values = {"active", "on'hold"}},
  };

  if (postgresDialect.renderColumnType(sizedString) != "varchar(120)" ||
      postgresDialect.renderColumnType(decimal) != "decimal(10,2)" ||
      postgresDialect.renderColumnType(zonedDateTime) != "timestamp with time zone" ||
      postgresDialect.renderColumnType(postgresEnum) != "\"public\".\"account_status\"" ||
      mySqlDialect.renderColumnType(sizedString) != "varchar(120)" ||
      mySqlDialect.renderColumnType({.kind = worm::core::ColumnTypeKind::Uuid}) != "char(36)" ||
      mySqlDialect.renderColumnType(mySqlEnum) != "enum('active','on''hold')" ||
      sqliteDialect.renderColumnType(decimal) != "real" ||
      sqlServerDialect.renderColumnType(sizedString) != "nvarchar(120)" ||
      sqlServerDialect.renderColumnType(zonedDateTime) != "datetimeoffset") {
    std::cerr << "Dialect rendered an unexpected column type.\n";
    return 1;
  }

  try {
    static_cast<void>(postgresDialect.renderColumnType({}));
    std::cerr << "Dialect accepted an unknown column type.\n";
    return 1;
  } catch (const worm::SqlBuildException&) {}

  const std::vector<std::unique_ptr<worm::core::Dialect>> dialects = [] {
    std::vector<std::unique_ptr<worm::core::Dialect>> values;
    values.push_back(std::make_unique<worm::core::PostgresDialect>());
    values.push_back(std::make_unique<worm::core::MySqlDialect>());
    values.push_back(std::make_unique<worm::core::SqliteDialect>());
    values.push_back(std::make_unique<worm::core::SqlServerDialect>());
    return values;
  }();

  for (const auto& dialect : dialects) {
    if (dialect->quoteIdentifier("id").empty()) {
      std::cerr << "Dialect polymorphic usage returned an empty identifier.\n";
      return 1;
    }
  }

  return 0;
}
