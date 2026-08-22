#include <core/model/schema-snapshot.hpp>

#include <algorithm>

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
    const auto kinds = {
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

    for (const auto kind : kinds) {
      if (columnTypeKindName(kind) == name)
        return kind;
    }
    
    return std::nullopt;
  }

  const SchemaColumnSnapshot* SchemaTableSnapshot::findColumn(std::string_view columnName) const noexcept
  {
    const auto column = std::find_if(
      columns.begin(), columns.end(), [columnName](const auto& candidate) { return candidate.name == columnName; });

    return column == columns.end() ? nullptr : &*column;
  }

  const SchemaTableSnapshot* SchemaSnapshot::findTable(
    std::string_view schema, std::string_view tableName) const noexcept
  {
    const auto table = std::find_if(tables.begin(), tables.end(), [schema, tableName](const auto& candidate) {
      return candidate.schema == schema && candidate.name == tableName;
    });

    return table == tables.end() ? nullptr : &*table;
  }
} // namespace worm::core
