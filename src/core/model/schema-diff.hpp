#pragma once

#include <core/model/entity-metadata.hpp>
#include <core/model/schema-metadata.hpp>
#include <core/model/schema-snapshot.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace worm::core
{
  enum class SchemaDifferenceKind
  {
    MissingTable,
    UnexpectedTable,
    MissingColumn,
    UnexpectedColumn,
    ColumnTypeMismatch,
    NullableMismatch,
    GeneratedMismatch,
    UniqueMismatch,
    DefaultExpressionMismatch,
    MissingPrimaryKey,
    PrimaryKeyMismatch
  };

  struct SchemaDifference
  {
    SchemaDifferenceKind kind;
    std::string schema;
    std::string table;
    std::string column;
    std::string expected;
    std::string actual;
  };

  [[nodiscard]]
  bool columnTypesCompatible(const ColumnType& expected, const ColumnType& actual);

  namespace detail
  {
    [[nodiscard]]
    inline std::string boolValue(bool value)
    {
      return value ? "true" : "false";
    }

    [[nodiscard]]
    inline SchemaDifference differenceFor(SchemaDifferenceKind kind, Table table)
    {
      return {
        .kind = kind,
        .schema = std::string{table.schema().name()},
        .table = std::string{table.name()},
      };
    }

    [[nodiscard]]
    inline bool samePrimaryKeyColumns(const PrimaryKey& expected, const PrimaryKey& actual) noexcept
    {
      if (expected.columns().size() != actual.columns().size()) {
        return false;
      }

      for (std::size_t index = 0; index < expected.columns().size(); ++index) {
        if (expected.columns()[index].columnName != actual.columns()[index].columnName) {
          return false;
        }
      }

      return true;
    }

    template <typename T, typename Field>
    void appendExpectedColumnDifference(
      const TableMetadata& existing,
      const Field& field,
      std::vector<SchemaDifference>& differences)
    {
      const ColumnMetadata* actualColumn = existing.findColumn(field.columnName());
      if (actualColumn == nullptr) {
        SchemaDifference difference = differenceFor(SchemaDifferenceKind::MissingColumn, table_of<T>());
        difference.column = field.columnName();
        differences.push_back(std::move(difference));
        return;
      }

      const auto& expectedMetadata = field.metadata();
      if (expectedMetadata.nullable != actualColumn->nullable) {
        SchemaDifference difference = differenceFor(SchemaDifferenceKind::NullableMismatch, table_of<T>());
        difference.column = field.columnName();
        difference.expected = boolValue(expectedMetadata.nullable);
        difference.actual = boolValue(actualColumn->nullable);
        differences.push_back(std::move(difference));
      }

      if (expectedMetadata.generated != actualColumn->generated) {
        SchemaDifference difference = differenceFor(SchemaDifferenceKind::GeneratedMismatch, table_of<T>());
        difference.column = field.columnName();
        difference.expected = boolValue(expectedMetadata.generated);
        difference.actual = boolValue(actualColumn->generated);
        differences.push_back(std::move(difference));
      }

      if (expectedMetadata.unique != actualColumn->unique) {
        SchemaDifference difference = differenceFor(SchemaDifferenceKind::UniqueMismatch, table_of<T>());
        difference.column = field.columnName();
        difference.expected = boolValue(expectedMetadata.unique);
        difference.actual = boolValue(actualColumn->unique);
        differences.push_back(std::move(difference));
      }
    }

    template <typename T>
    [[nodiscard]]
    bool hasPersistentColumn(std::string_view columnName) noexcept
    {
      bool found = false;

      std::apply(
        [&](const auto&... fields) {
          (
            [&] {
              if (fields.columnName() == columnName) {
                found = true;
              }
            }(),
            ...);
        },
        persistent_fields_of<T>());

      return found;
    }
  } // namespace detail

  template <PersistableEntity T>
  [[nodiscard]]
  std::vector<SchemaDifference> compareEntityWithTable(const TableMetadata& existing)
  {
    std::vector<SchemaDifference> differences;
    const Table expectedTable = table_of<T>();

    if (existing.table() != expectedTable) {
      SchemaDifference difference = detail::differenceFor(SchemaDifferenceKind::MissingTable, expectedTable);
      difference.expected = expectedTable.name();
      difference.actual = existing.table().name();
      differences.push_back(std::move(difference));

      return differences;
    }

    std::apply(
      [&](const auto&... fields) { (detail::appendExpectedColumnDifference<T>(existing, fields, differences), ...); },
      persistent_fields_of<T>());

    for (const ColumnMetadata& column : existing.columns()) {
      if (!detail::hasPersistentColumn<T>(column.columnName)) {
        SchemaDifference difference = detail::differenceFor(SchemaDifferenceKind::UnexpectedColumn, expectedTable);
        difference.column = column.columnName;
        differences.push_back(std::move(difference));
      }
    }

    const PrimaryKey expectedPrimaryKey = std::remove_cvref_t<T>::primaryKey();
    if (!existing.primaryKey().has_value()) {
      SchemaDifference difference = detail::differenceFor(SchemaDifferenceKind::MissingPrimaryKey, expectedTable);
      difference.expected = expectedPrimaryKey.name();
      differences.push_back(std::move(difference));
    } else if (!detail::samePrimaryKeyColumns(expectedPrimaryKey, existing.primaryKey().value())) {
      SchemaDifference difference = detail::differenceFor(SchemaDifferenceKind::PrimaryKeyMismatch, expectedTable);
      difference.expected = expectedPrimaryKey.name();
      difference.actual = existing.primaryKey()->name();
      differences.push_back(std::move(difference));
    }

    return differences;
  }

  template <PersistableEntity T>
  [[nodiscard]]
  std::vector<SchemaDifference> compareEntityWithSchema(const SchemaMetadata& existing)
  {
    const TableMetadata* table = existing.findTable(table_of<T>());
    if (table == nullptr) {
      SchemaDifference difference = detail::differenceFor(SchemaDifferenceKind::MissingTable, table_of<T>());
      difference.expected = table_of<T>().name();
      return {std::move(difference)};
    }

    return compareEntityWithTable<T>(*table);
  }

  [[nodiscard]]
  std::vector<SchemaDifference> compareSchemas(const SchemaMetadata& expected, const SchemaSnapshot& actual);
} // namespace worm::core
