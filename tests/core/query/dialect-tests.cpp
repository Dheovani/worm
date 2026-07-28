#include <core/query/dialect.hpp>

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

  if (!hasDialectContract(postgresDialect, 3, "$3", "weird\"name", "\"users\"", "\"weird\"\"name\"")) {
    return 1;
  }

  if (!hasDialectContract(mySqlDialect, 3, "?", "weird`name", "`users`", "`weird``name`")) {
    return 1;
  }

  if (!hasDialectContract(sqliteDialect, 3, "?", "weird\"name", "\"users\"", "\"weird\"\"name\"")) {
    return 1;
  }

  const std::vector<std::unique_ptr<worm::core::Dialect>> dialects = [] {
    std::vector<std::unique_ptr<worm::core::Dialect>> values;
    values.push_back(std::make_unique<worm::core::PostgresDialect>());
    values.push_back(std::make_unique<worm::core::MySqlDialect>());
    values.push_back(std::make_unique<worm::core::SqliteDialect>());
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
