#pragma once

#include <core/model/schema.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace worm::core
{
  struct SchemaColumnSnapshot
  {
    std::string name;
    ColumnType type;
    bool nullable{true};
    bool generated{false};
    bool unique{false};

    friend bool operator==(const SchemaColumnSnapshot&, const SchemaColumnSnapshot&) = default;
  };

  struct SchemaTableSnapshot
  {
    std::string schema;
    std::string name;
    std::vector<SchemaColumnSnapshot> columns;
    std::vector<std::string> primaryKey;
    std::vector<std::string> foreignKey;
    std::vector<std::string> indexes;

    [[nodiscard]]
    const SchemaColumnSnapshot* findColumn(std::string_view columnName) const noexcept;

    friend bool operator==(const SchemaTableSnapshot&, const SchemaTableSnapshot&) = default;
  };

  struct SchemaSnapshot
  {
    std::vector<SchemaTableSnapshot> tables;

    [[nodiscard]]
    const SchemaTableSnapshot* findTable(std::string_view schema, std::string_view tableName) const noexcept;

    friend bool operator==(const SchemaSnapshot&, const SchemaSnapshot&) = default;
  };
} // namespace worm::core
