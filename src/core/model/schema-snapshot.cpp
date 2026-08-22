#include <core/model/schema-snapshot.hpp>

#include <algorithm>

namespace worm::core
{
  const SchemaColumnSnapshot* SchemaTableSnapshot::findColumn(std::string_view columnName) const noexcept
  {
    const auto column = std::find_if(
      columns.begin(), columns.end(), [columnName](const auto& candidate) { return candidate.name == columnName; });

    return column == columns.end() ? nullptr : &*column;
  }

  const SchemaTableSnapshot* SchemaSnapshot::findTable(
    std::string_view schema,
    std::string_view tableName) const noexcept
  {
    const auto table = std::find_if(tables.begin(), tables.end(), [schema, tableName](const auto& candidate) {
      return candidate.schema == schema && candidate.name == tableName;
    });

    return table == tables.end() ? nullptr : &*table;
  }
} // namespace worm::core
