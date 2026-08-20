#pragma once

#include <core/model/constraint.hpp>
#include <core/model/schema.hpp>

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace worm::core
{
  class TableMetadata
  {
  public:
    explicit TableMetadata(
      Table table,
      std::vector<Column> columns = {},
      std::optional<PrimaryKey> primaryKey = std::nullopt,
      std::vector<Index> indexes = {},
      std::vector<ForeignKey> foreignKeys = {})
      : table_(table),
        columns_(std::move(columns)),
        primaryKey_(std::move(primaryKey)),
        indexes_(std::move(indexes)),
        foreignKeys_(std::move(foreignKeys))
    {
    }

    [[nodiscard]]
    const Table& table() const noexcept
    {
      return table_;
    }

    [[nodiscard]]
    const std::vector<Column>& columns() const noexcept
    {
      return columns_;
    }

    [[nodiscard]]
    const std::optional<PrimaryKey>& primaryKey() const noexcept
    {
      return primaryKey_;
    }

    [[nodiscard]]
    const std::vector<Index>& indexes() const noexcept
    {
      return indexes_;
    }

    [[nodiscard]]
    const std::vector<ForeignKey>& foreignKeys() const noexcept
    {
      return foreignKeys_;
    }

    [[nodiscard]]
    const Column* findColumn(std::string_view name) const noexcept
    {
      for (const Column& column : columns_) {
        if (column.columnName == name) {
          return &column;
        }
      }

      return nullptr;
    }

  private:
    Table table_;
    std::vector<Column> columns_;
    std::optional<PrimaryKey> primaryKey_;
    std::vector<Index> indexes_;
    std::vector<ForeignKey> foreignKeys_;
  };

  class SchemaMetadata
  {
  public:
    explicit SchemaMetadata(Schema schema, std::vector<TableMetadata> tables = {})
      : schema_(schema),
        tables_(std::move(tables))
    {
    }

    [[nodiscard]]
    const Schema& schema() const noexcept
    {
      return schema_;
    }

    [[nodiscard]]
    const std::vector<TableMetadata>& tables() const noexcept
    {
      return tables_;
    }

    [[nodiscard]]
    const TableMetadata* findTable(Table table) const noexcept
    {
      for (const TableMetadata& metadata : tables_) {
        if (metadata.table() == table) {
          return &metadata;
        }
      }

      return nullptr;
    }

  private:
    Schema schema_;
    std::vector<TableMetadata> tables_;
  };
} // namespace worm::core
