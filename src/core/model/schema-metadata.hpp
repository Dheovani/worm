#pragma once

#include <core/model/constraint.hpp>
#include <core/model/schema.hpp>

#include <optional>
#include <string_view>
#include <vector>

namespace worm::core
{
  class ColumnMetadata : public Column
  {
  public:
    ColumnMetadata(Column column, ColumnType type = {});

    [[nodiscard]]
    const ColumnType& type() const noexcept;

  private:
    ColumnType type_;
  };

  class TableMetadata
  {
  public:
    explicit TableMetadata(Table table,
      std::vector<ColumnMetadata> columns = {},
      std::optional<PrimaryKey> primaryKey = std::nullopt,
      std::vector<Index> indexes = {},
      std::vector<ForeignKey> foreignKeys = {});

    [[nodiscard]]
    const Table& table() const noexcept;

    [[nodiscard]]
    const std::vector<ColumnMetadata>& columns() const noexcept;

    [[nodiscard]]
    const std::optional<PrimaryKey>& primaryKey() const noexcept;

    [[nodiscard]]
    const std::vector<Index>& indexes() const noexcept;

    [[nodiscard]]
    const std::vector<ForeignKey>& foreignKeys() const noexcept;

    [[nodiscard]]
    const ColumnMetadata* findColumn(std::string_view name) const noexcept;

  private:
    Table table_;
    std::vector<ColumnMetadata> columns_;
    std::optional<PrimaryKey> primaryKey_;
    std::vector<Index> indexes_;
    std::vector<ForeignKey> foreignKeys_;
  };

  class SchemaMetadata
  {
  public:
    explicit SchemaMetadata(Schema schema, std::vector<TableMetadata> tables = {});

    [[nodiscard]]
    const Schema& schema() const noexcept;

    [[nodiscard]]
    const std::vector<TableMetadata>& tables() const noexcept;

    [[nodiscard]]
    const TableMetadata* findTable(Table table) const noexcept;

  private:
    Schema schema_;
    std::vector<TableMetadata> tables_;
  };
} // namespace worm::core
