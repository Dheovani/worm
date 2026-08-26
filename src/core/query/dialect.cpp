#include <core/query/dialect.hpp>

#include <errors/sql-build-exception.hpp>

namespace worm::core
{
  namespace
  {
    [[nodiscard]]
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

    [[nodiscard]]
    std::string decimalType(const ColumnType& type)
    {
      if (!type.precision.has_value()) {
        return "decimal";
      }

      if (type.precision.value() == 0 || type.scale.value_or(0) > type.precision.value()) {
        throw worm::SqlBuildException("Invalid decimal precision or scale.");
      }

      std::string result = "decimal(" + std::to_string(type.precision.value());
      if (type.scale.has_value()) {
        result += "," + std::to_string(type.scale.value());
      }
      return result + ")";
    }

    [[noreturn]]
    void throwUnsupportedType(std::string_view dialect)
    {
      throw worm::SqlBuildException("Unsupported {} column type.", dialect);
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

  std::string PostgresDialect::renderColumnType(const ColumnType& type) const
  {
    switch (type.kind) {
    case ColumnTypeKind::Boolean:
      return "boolean";
    case ColumnTypeKind::Int16:
      return "smallint";
    case ColumnTypeKind::Int32:
      return "integer";
    case ColumnTypeKind::Int64:
      return "bigint";
    case ColumnTypeKind::Float32:
      return "real";
    case ColumnTypeKind::Float64:
      return "double precision";
    case ColumnTypeKind::Decimal:
      return decimalType(type);
    case ColumnTypeKind::String:
      return type.length.has_value() ? "varchar(" + std::to_string(type.length.value()) + ")" : "text";
    case ColumnTypeKind::Binary:
      return "bytea";
    case ColumnTypeKind::Date:
      return "date";
    case ColumnTypeKind::Time:
      return type.withTimeZone ? "time with time zone" : "time";
    case ColumnTypeKind::DateTime:
      return type.withTimeZone ? "timestamp with time zone" : "timestamp";
    case ColumnTypeKind::Uuid:
      return "uuid";
    case ColumnTypeKind::Json:
      return "jsonb";
    case ColumnTypeKind::Unknown:
      break;
    }

    throwUnsupportedType("PostgreSQL");
  }

  std::string MySqlDialect::placeholder(std::size_t) const
  {
    return "?";
  }

  std::string MySqlDialect::quoteIdentifier(std::string_view identifier) const
  {
    return quote(identifier, '`');
  }

  std::string MySqlDialect::renderColumnType(const ColumnType& type) const
  {
    switch (type.kind) {
    case ColumnTypeKind::Boolean:
      return "boolean";
    case ColumnTypeKind::Int16:
      return "smallint";
    case ColumnTypeKind::Int32:
      return "int";
    case ColumnTypeKind::Int64:
      return "bigint";
    case ColumnTypeKind::Float32:
      return "float";
    case ColumnTypeKind::Float64:
      return "double";
    case ColumnTypeKind::Decimal:
      return decimalType(type);
    case ColumnTypeKind::String:
      return type.length.has_value() ? "varchar(" + std::to_string(type.length.value()) + ")" : "text";
    case ColumnTypeKind::Binary:
      return "blob";
    case ColumnTypeKind::Date:
      return "date";
    case ColumnTypeKind::Time:
      return "time";
    case ColumnTypeKind::DateTime:
      return "datetime";
    case ColumnTypeKind::Uuid:
      return "char(36)";
    case ColumnTypeKind::Json:
      return "json";
    case ColumnTypeKind::Unknown:
      break;
    }

    throwUnsupportedType("MySQL");
  }

  std::string SqliteDialect::placeholder(std::size_t) const
  {
    return "?";
  }

  std::string SqliteDialect::quoteIdentifier(std::string_view identifier) const
  {
    return quote(identifier, '"');
  }

  std::string SqliteDialect::renderColumnType(const ColumnType& type) const
  {
    switch (type.kind) {
    case ColumnTypeKind::Boolean:
    case ColumnTypeKind::Int16:
    case ColumnTypeKind::Int32:
    case ColumnTypeKind::Int64:
      return "integer";
    case ColumnTypeKind::Float32:
    case ColumnTypeKind::Float64:
    case ColumnTypeKind::Decimal:
      return "real";
    case ColumnTypeKind::Binary:
      return "blob";
    case ColumnTypeKind::String:
    case ColumnTypeKind::Date:
    case ColumnTypeKind::Time:
    case ColumnTypeKind::DateTime:
    case ColumnTypeKind::Uuid:
    case ColumnTypeKind::Json:
      return "text";
    case ColumnTypeKind::Unknown:
      break;
    }

    throwUnsupportedType("SQLite");
  }

  std::string SqlServerDialect::placeholder(std::size_t) const
  {
    return "?";
  }

  std::string SqlServerDialect::quoteIdentifier(std::string_view identifier) const
  {
    std::string quoted{"["};
    quoted.reserve(identifier.size() + 2);

    for (const char character : identifier) {
      quoted += character;
      if (character == ']') {
        quoted += ']';
      }
    }

    quoted += ']';
    return quoted;
  }

  std::string SqlServerDialect::renderColumnType(const ColumnType& type) const
  {
    switch (type.kind) {
    case ColumnTypeKind::Boolean:
      return "bit";
    case ColumnTypeKind::Int16:
      return "smallint";
    case ColumnTypeKind::Int32:
      return "int";
    case ColumnTypeKind::Int64:
      return "bigint";
    case ColumnTypeKind::Float32:
      return "real";
    case ColumnTypeKind::Float64:
      return "float";
    case ColumnTypeKind::Decimal:
      return decimalType(type);
    case ColumnTypeKind::String:
      return type.length.has_value() ? "nvarchar(" + std::to_string(type.length.value()) + ")" : "nvarchar(max)";
    case ColumnTypeKind::Binary:
      return type.length.has_value() ? "varbinary(" + std::to_string(type.length.value()) + ")" : "varbinary(max)";
    case ColumnTypeKind::Date:
      return "date";
    case ColumnTypeKind::Time:
      return "time";
    case ColumnTypeKind::DateTime:
      return type.withTimeZone ? "datetimeoffset" : "datetime2";
    case ColumnTypeKind::Uuid:
      return "uniqueidentifier";
    case ColumnTypeKind::Json:
      return "nvarchar(max)";
    case ColumnTypeKind::Unknown:
      break;
    }

    throwUnsupportedType("SQL Server");
  }
} // namespace worm::core
