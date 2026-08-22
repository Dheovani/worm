#include <core/model/schema-metadata.hpp>

namespace worm::core
{
  ColumnMetadata::ColumnMetadata(Column column, ColumnType type)
    : Column(std::move(column)),
      type_(std::move(type))
  {}

  const ColumnType& ColumnMetadata::type() const noexcept
  {
    return type_;
  }

  TableMetadata::TableMetadata(
    Table table,
    std::vector<ColumnMetadata> columns,
    std::optional<PrimaryKey> primaryKey,
    std::vector<Index> indexes,
    std::vector<ForeignKey> foreignKeys)
    : table_(table),
      columns_(std::move(columns)),
      primaryKey_(std::move(primaryKey)),
      indexes_(std::move(indexes)),
      foreignKeys_(std::move(foreignKeys))
  {}

  const Table& TableMetadata::table() const noexcept
  {
    return table_;
  }

  const std::vector<ColumnMetadata>& TableMetadata::columns() const noexcept
  {
    return columns_;
  }

  const std::optional<PrimaryKey>& TableMetadata::primaryKey() const noexcept
  {
    return primaryKey_;
  }

  const std::vector<Index>& TableMetadata::indexes() const noexcept
  {
    return indexes_;
  }

  const std::vector<ForeignKey>& TableMetadata::foreignKeys() const noexcept
  {
    return foreignKeys_;
  }

  const ColumnMetadata* TableMetadata::findColumn(std::string_view name) const noexcept
  {
    for (const ColumnMetadata& column : columns_) {
      if (column.columnName == name) {
        return &column;
      }
    }

    return nullptr;
  }

  SchemaMetadata::SchemaMetadata(Schema schema, std::vector<TableMetadata> tables)
    : schema_(schema),
      tables_(std::move(tables))
  {}

  const Schema& SchemaMetadata::schema() const noexcept
  {
    return schema_;
  }

  const std::vector<TableMetadata>& SchemaMetadata::tables() const noexcept
  {
    return tables_;
  }

  const TableMetadata* SchemaMetadata::findTable(Table table) const noexcept
  {
    for (const TableMetadata& metadata : tables_) {
      if (metadata.table() == table) {
        return &metadata;
      }
    }

    return nullptr;
  }
} // namespace worm::core
