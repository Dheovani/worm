#include <core/dialect.hpp>

namespace worm::core
{
  namespace
  {
    std::string quote(std::string_view identifier, char delimiter)
    {
      std::string quotedIdentifier;
      quotedIdentifier.reserve(identifier.size() + 2);
      quotedIdentifier += delimiter;

      for (const char character : identifier) {
        quotedIdentifier += character;

        if (character == delimiter) {
          quotedIdentifier += character;
        }
      }

      quotedIdentifier += delimiter;
      return quotedIdentifier;
    }
  } // namespace

  std::string PostgresDialect::placeholder(std::size_t index) const
  {
    return "$" + std::to_string(index);
  }

  std::string PostgresDialect::quoteIdentifier(std::string_view identifier) const
  {
    return quote(identifier, '"');
  }

  std::string MySqlDialect::placeholder(std::size_t) const
  {
    return "?";
  }

  std::string MySqlDialect::quoteIdentifier(std::string_view identifier) const
  {
    return quote(identifier, '`');
  }

  std::string SqliteDialect::placeholder(std::size_t) const
  {
    return "?";
  }

  std::string SqliteDialect::quoteIdentifier(std::string_view identifier) const
  {
    return quote(identifier, '"');
  }

} // namespace worm::core
