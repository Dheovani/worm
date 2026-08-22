#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace worm::core
{
  enum class ColumnTypeKind
  {
    Boolean,
    Int16,
    Int32,
    Int64,
    Float32,
    Float64,
    Decimal,
    String,
    Binary,
    Date,
    Time,
    DateTime,
    Uuid,
    Json,
    Unknown
  };

  struct ColumnType
  {
    ColumnTypeKind kind{ColumnTypeKind::Unknown};
    std::string nativeName;
    std::optional<std::size_t> length;
    std::optional<std::size_t> precision;
    std::optional<std::size_t> scale;
    bool unsignedValue{false};
    bool withTimeZone{false};

    friend bool operator==(const ColumnType&, const ColumnType&) = default;
  };

  [[nodiscard]]
  std::string_view columnTypeKindName(ColumnTypeKind kind) noexcept;

  [[nodiscard]]
  std::optional<ColumnTypeKind> parseColumnTypeKind(std::string_view name) noexcept;

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
