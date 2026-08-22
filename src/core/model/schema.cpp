#include <core/model/schema.hpp>

namespace worm::core
{
  std::string_view columnTypeKindName(ColumnTypeKind kind) noexcept
  {
    switch (kind) {
    case ColumnTypeKind::Boolean:
      return "boolean";
    case ColumnTypeKind::Int16:
      return "int16";
    case ColumnTypeKind::Int32:
      return "int32";
    case ColumnTypeKind::Int64:
      return "int64";
    case ColumnTypeKind::Float32:
      return "float32";
    case ColumnTypeKind::Float64:
      return "float64";
    case ColumnTypeKind::Decimal:
      return "decimal";
    case ColumnTypeKind::String:
      return "string";
    case ColumnTypeKind::Binary:
      return "binary";
    case ColumnTypeKind::Date:
      return "date";
    case ColumnTypeKind::Time:
      return "time";
    case ColumnTypeKind::DateTime:
      return "datetime";
    case ColumnTypeKind::Uuid:
      return "uuid";
    case ColumnTypeKind::Json:
      return "json";
    case ColumnTypeKind::Unknown:
      return "unknown";
    }

    return "unknown";
  }

  std::optional<ColumnTypeKind> parseColumnTypeKind(std::string_view name) noexcept
  {
    constexpr ColumnTypeKind kinds[]{
      ColumnTypeKind::Boolean,
      ColumnTypeKind::Int16,
      ColumnTypeKind::Int32,
      ColumnTypeKind::Int64,
      ColumnTypeKind::Float32,
      ColumnTypeKind::Float64,
      ColumnTypeKind::Decimal,
      ColumnTypeKind::String,
      ColumnTypeKind::Binary,
      ColumnTypeKind::Date,
      ColumnTypeKind::Time,
      ColumnTypeKind::DateTime,
      ColumnTypeKind::Uuid,
      ColumnTypeKind::Json,
      ColumnTypeKind::Unknown,
    };

    for (const ColumnTypeKind kind : kinds) {
      if (columnTypeKindName(kind) == name) {
        return kind;
      }
    }

    return std::nullopt;
  }
} // namespace worm::core
