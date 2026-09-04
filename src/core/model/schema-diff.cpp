#include <core/model/schema-diff.hpp>

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace worm::core
{
  namespace
  {
    [[nodiscard]]
    SchemaDifference differenceFor(SchemaDifferenceKind kind, const Table& table)
    {
      return {
        .kind = kind,
        .schema = std::string{table.schema().name()},
        .table = std::string{table.name()},
      };
    }

    [[nodiscard]]
    SchemaDifference differenceFor(SchemaDifferenceKind kind, const SchemaTableSnapshot& table)
    {
      return {.kind = kind, .schema = table.schema, .table = table.name};
    }

    [[nodiscard]]
    std::string typeDescription(const ColumnType& type)
    {
      std::string description{columnTypeKindName(type.kind)};
      if (type.length.has_value()) {
        description += "(" + std::to_string(*type.length) + ")";
      } else if (type.precision.has_value()) {
        description += "(" + std::to_string(*type.precision);
        if (type.scale.has_value()) {
          description += "," + std::to_string(*type.scale);
        }
        description += ")";
      }
      if (type.unsignedValue) {
        description += " unsigned";
      }
      if (type.withTimeZone) {
        description += " with time zone";
      }
      return description;
    }

    [[nodiscard]]
    std::vector<std::string> primaryKeyColumns(const TableMetadata& table)
    {
      std::vector<std::string> columns;
      if (!table.primaryKey().has_value()) {
        return columns;
      }
      for (const Column& column : table.primaryKey()->columns()) {
        columns.emplace_back(column.columnName);
      }
      return columns;
    }

    void compareTable(
      const TableMetadata& expected,
      const SchemaTableSnapshot& actual,
      std::vector<SchemaDifference>& differences)
    {
      for (const ColumnMetadata& expectedColumn : expected.columns()) {
        const SchemaColumnSnapshot* actualColumn = actual.findColumn(expectedColumn.columnName);
        if (actualColumn == nullptr) {
          SchemaDifference difference = differenceFor(SchemaDifferenceKind::MissingColumn, expected.table());
          difference.column = expectedColumn.columnName;
          differences.push_back(std::move(difference));
          continue;
        }

        if (!columnTypesCompatible(expectedColumn.type(), actualColumn->type)) {
          SchemaDifference difference = differenceFor(SchemaDifferenceKind::ColumnTypeMismatch, expected.table());
          difference.column = expectedColumn.columnName;
          difference.expected = typeDescription(expectedColumn.type());
          difference.actual = typeDescription(actualColumn->type);
          differences.push_back(std::move(difference));
        }
        if (expectedColumn.nullable != actualColumn->nullable) {
          SchemaDifference difference = differenceFor(SchemaDifferenceKind::NullableMismatch, expected.table());
          difference.column = expectedColumn.columnName;
          difference.expected = detail::boolValue(expectedColumn.nullable);
          difference.actual = detail::boolValue(actualColumn->nullable);
          differences.push_back(std::move(difference));
        }
        if (expectedColumn.generated != actualColumn->generated) {
          SchemaDifference difference = differenceFor(SchemaDifferenceKind::GeneratedMismatch, expected.table());
          difference.column = expectedColumn.columnName;
          difference.expected = detail::boolValue(expectedColumn.generated);
          difference.actual = detail::boolValue(actualColumn->generated);
          differences.push_back(std::move(difference));
        }
        if (expectedColumn.unique != actualColumn->unique) {
          SchemaDifference difference = differenceFor(SchemaDifferenceKind::UniqueMismatch, expected.table());
          difference.column = expectedColumn.columnName;
          difference.expected = detail::boolValue(expectedColumn.unique);
          difference.actual = detail::boolValue(actualColumn->unique);
          differences.push_back(std::move(difference));
        }
        if (!expectedColumn.defaultExpression.empty() &&
            expectedColumn.defaultExpression != actualColumn->defaultExpression.value_or("")) {
          SchemaDifference difference =
            differenceFor(SchemaDifferenceKind::DefaultExpressionMismatch, expected.table());
          difference.column = expectedColumn.columnName;
          difference.expected = expectedColumn.defaultExpression;
          difference.actual = actualColumn->defaultExpression.value_or("");
          differences.push_back(std::move(difference));
        }
      }

      for (const SchemaColumnSnapshot& actualColumn : actual.columns) {
        if (expected.findColumn(actualColumn.name) == nullptr) {
          SchemaDifference difference = differenceFor(SchemaDifferenceKind::UnexpectedColumn, expected.table());
          difference.column = actualColumn.name;
          differences.push_back(std::move(difference));
        }
      }

      const std::vector<std::string> expectedPrimaryKey = primaryKeyColumns(expected);
      if (expectedPrimaryKey != actual.primaryKey) {
        SchemaDifference difference = differenceFor(
          actual.primaryKey.empty() ? SchemaDifferenceKind::MissingPrimaryKey
                                    : SchemaDifferenceKind::PrimaryKeyMismatch,
          expected.table());
        differences.push_back(std::move(difference));
      }
    }
  } // namespace

  bool columnTypesCompatible(const ColumnType& expected, const ColumnType& actual)
  {
    if (expected.kind == ColumnTypeKind::Unknown) {
      return true;
    }

    return expected.kind == actual.kind && (!expected.length.has_value() || expected.length == actual.length) &&
           (!expected.precision.has_value() || expected.precision == actual.precision) &&
           (!expected.scale.has_value() || expected.scale == actual.scale) &&
           expected.unsignedValue == actual.unsignedValue && expected.withTimeZone == actual.withTimeZone &&
           (expected.kind != ColumnTypeKind::Enum || expected.enumeration == actual.enumeration);
  }

  std::vector<SchemaDifference> compareSchemas(const SchemaMetadata& expected, const SchemaSnapshot& actual)
  {
    std::vector<SchemaDifference> differences;
    for (const TableMetadata& expectedTable : expected.tables()) {
      const Table& table = expectedTable.table();
      const SchemaTableSnapshot* actualTable = actual.findTable(table.schema().name(), table.name());
      if (actualTable == nullptr) {
        differences.push_back(differenceFor(SchemaDifferenceKind::MissingTable, table));
        continue;
      }
      compareTable(expectedTable, *actualTable, differences);
    }

    for (const SchemaTableSnapshot& actualTable : actual.tables) {
      const Table table{Schema{actualTable.schema}, actualTable.name};
      if (expected.findTable(table) == nullptr) {
        differences.push_back(differenceFor(SchemaDifferenceKind::UnexpectedTable, actualTable));
      }
    }
    return differences;
  }
} // namespace worm::core
