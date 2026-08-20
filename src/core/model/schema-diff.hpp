#pragma once

#include <core/model/entity-metadata.hpp>
#include <core/model/schema-metadata.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

namespace worm::core
{
  enum class SchemaDifferenceKind
  {
    MissingTable,
    MissingColumn,
    UnexpectedColumn,
    NullableMismatch,
    GeneratedMismatch,
    UniqueMismatch,
    MissingPrimaryKey,
    PrimaryKeyMismatch
  };

  struct SchemaDifference
  {
    SchemaDifferenceKind kind;
    Table table;
    std::string column;
    std::string expected;
    std::string actual;
  };

  namespace detail
  {
    [[nodiscard]]
    inline std::string boolValue(bool value)
    {
      return value ? "true" : "false";
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
      const Column* actualColumn = existing.findColumn(field.columnName());
      if (actualColumn == nullptr) {
        differences.push_back({
          .kind = SchemaDifferenceKind::MissingColumn,
          .table = table_of<T>(),
          .column = std::string{field.columnName()},
        });
        return;
      }

      const auto& expectedMetadata = field.metadata();
      if (expectedMetadata.nullable != actualColumn->nullable) {
        differences.push_back({
          .kind = SchemaDifferenceKind::NullableMismatch,
          .table = table_of<T>(),
          .column = std::string{field.columnName()},
          .expected = boolValue(expectedMetadata.nullable),
          .actual = boolValue(actualColumn->nullable),
        });
      }

      if (expectedMetadata.generated != actualColumn->generated) {
        differences.push_back({
          .kind = SchemaDifferenceKind::GeneratedMismatch,
          .table = table_of<T>(),
          .column = std::string{field.columnName()},
          .expected = boolValue(expectedMetadata.generated),
          .actual = boolValue(actualColumn->generated),
        });
      }

      if (expectedMetadata.unique != actualColumn->unique) {
        differences.push_back({
          .kind = SchemaDifferenceKind::UniqueMismatch,
          .table = table_of<T>(),
          .column = std::string{field.columnName()},
          .expected = boolValue(expectedMetadata.unique),
          .actual = boolValue(actualColumn->unique),
        });
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
      differences.push_back({
        .kind = SchemaDifferenceKind::MissingTable,
        .table = expectedTable,
        .expected = std::string{expectedTable.name()},
        .actual = std::string{existing.table().name()},
      });

      return differences;
    }

    std::apply(
      [&](const auto&... fields) {
        (detail::appendExpectedColumnDifference<T>(existing, fields, differences), ...);
      },
      persistent_fields_of<T>());

    for (const Column& column : existing.columns()) {
      if (!detail::hasPersistentColumn<T>(column.columnName)) {
        differences.push_back({
          .kind = SchemaDifferenceKind::UnexpectedColumn,
          .table = expectedTable,
          .column = std::string{column.columnName},
        });
      }
    }

    const PrimaryKey expectedPrimaryKey = std::remove_cvref_t<T>::primaryKey();
    if (!existing.primaryKey().has_value()) {
      differences.push_back({
        .kind = SchemaDifferenceKind::MissingPrimaryKey,
        .table = expectedTable,
        .expected = std::string{expectedPrimaryKey.name()},
      });
    } else if (!detail::samePrimaryKeyColumns(expectedPrimaryKey, existing.primaryKey().value())) {
      differences.push_back({
        .kind = SchemaDifferenceKind::PrimaryKeyMismatch,
        .table = expectedTable,
        .expected = std::string{expectedPrimaryKey.name()},
        .actual = std::string{existing.primaryKey()->name()},
      });
    }

    return differences;
  }

  template <PersistableEntity T>
  [[nodiscard]]
  std::vector<SchemaDifference> compareEntityWithSchema(const SchemaMetadata& existing)
  {
    const TableMetadata* table = existing.findTable(table_of<T>());
    if (table == nullptr) {
      return {{
        .kind = SchemaDifferenceKind::MissingTable,
        .table = table_of<T>(),
        .expected = std::string{table_of<T>().name()},
      }};
    }

    return compareEntityWithTable<T>(*table);
  }
} // namespace worm::core
