#include "seed.hpp"

#include <connection/transaction.hpp>
#include <core/model/column.hpp>
#include <core/model/schema.hpp>
#include <core/persistence/repository.hpp>
#include <core/query/expression.hpp>
#include <reflection/field.hpp>
#include <utils/dependency-injection.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <memory>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <errors/invalid-cli-argument-exception.hpp>
#include <helpers/connection.hpp>

namespace worm::cli::database
{
  namespace
  {
    using Clock = std::chrono::steady_clock;

    struct SeedRecord
    {
      std::int64_t id{};

      static constexpr core::Table table() noexcept
      {
        return core::Table{"_worm_seed_record"};
      }

      static constexpr core::PrimaryKey primaryKey() noexcept
      {
        return core::PrimaryKey{"pk_worm_seed_record", {core::Column{"id", table()}}};
      }

      static constexpr auto reflect() noexcept
      {
        return std::tuple{reflection::field("id", &SeedRecord::id)};
      }
    };

    struct DependencyGraphNode
    {
      const core::SchemaTableSnapshot* table{};
      std::vector<std::size_t> dependencies;
    };

    struct TableReference
    {
      std::string schema;
      std::string table;
    };

    struct ForeignKeyReference
    {
      std::string column;
      TableReference table;
      std::string referencedColumn;
    };

    struct PopulationPlanColumn
    {
      std::string name;
      core::ColumnType type;
      core::Parameter value;
    };

    struct PopulationPlanRow
    {
      std::vector<PopulationPlanColumn> columns;
    };

    struct PopulationPlanTable
    {
      std::string schema;
      std::string name;
      std::vector<PopulationPlanRow> rows;
    };

    struct PopulationPlan
    {
      std::vector<PopulationPlanTable> tables;
    };

    [[nodiscard]]
    std::string tableKey(std::string_view schema, std::string_view table)
    {
      std::string key;
      key.reserve(schema.size() + table.size() + 1);
      key.append(schema);
      key.push_back('\x1f');
      key.append(table);
      return key;
    }

    [[nodiscard]]
    std::string tableLabel(const core::SchemaTableSnapshot& table)
    {
      return table.schema.empty() ? table.name : table.schema + "." + table.name;
    }

    [[nodiscard]]
    std::string_view trim(std::string_view value) noexcept
    {
      const std::size_t first = value.find_first_not_of(" \t\r\n");
      if (first == std::string_view::npos) {
        return {};
      }

      return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
    }

    [[nodiscard]]
    std::optional<TableReference> referencedTable(std::string_view foreignKey, std::string_view defaultSchema)
    {
      const std::size_t arrow = foreignKey.find("->");
      std::string_view reference =
        trim(arrow == std::string_view::npos ? foreignKey : foreignKey.substr(arrow + std::string_view{"->"}.size()));
      if (reference.empty()) {
        return std::nullopt;
      }

      if (arrow != std::string_view::npos) {
        const std::size_t columnSeparator = reference.rfind('.');
        if (columnSeparator == std::string_view::npos) {
          return std::nullopt;
        }
        reference = trim(reference.substr(0, columnSeparator));
      }

      const std::size_t schemaSeparator = reference.rfind('.');
      if (schemaSeparator == std::string_view::npos) {
        return TableReference{std::string{defaultSchema}, std::string{reference}};
      }

      return TableReference{
        std::string{trim(reference.substr(0, schemaSeparator))},
        std::string{trim(reference.substr(schemaSeparator + 1))},
      };
    }

    [[nodiscard]]
    std::optional<ForeignKeyReference> parseForeignKey(std::string_view foreignKey, std::string_view defaultSchema)
    {
      const std::size_t arrow = foreignKey.find("->");
      if (arrow == std::string_view::npos) {
        return std::nullopt;
      }

      const std::string_view column = trim(foreignKey.substr(0, arrow));
      const std::string_view reference = trim(foreignKey.substr(arrow + std::string_view{"->"}.size()));
      const std::size_t columnSeparator = reference.rfind('.');
      if (column.empty() || columnSeparator == std::string_view::npos) {
        return std::nullopt;
      }

      const std::string_view referencedColumn = trim(reference.substr(columnSeparator + 1));
      const std::optional<TableReference> table = referencedTable(foreignKey, defaultSchema);
      if (!table.has_value() || table->table.empty() || referencedColumn.empty()) {
        return std::nullopt;
      }

      return ForeignKeyReference{
        .column = std::string{column},
        .table = *table,
        .referencedColumn = std::string{referencedColumn},
      };
    }

    template <typename T>
    T generateColumnValue(std::size_t ordinal)
    {
      if constexpr (std::is_same_v<T, bool>) {
        return ordinal % 2 != 0;
      } else if constexpr (std::is_integral_v<T>) {
        return static_cast<T>(ordinal);
      } else if constexpr (std::is_floating_point_v<T>) {
        return static_cast<T>(ordinal);
      } else if constexpr (utils::is_string_like<T>) {
        return "_test_" + std::to_string(ordinal);
      } else if constexpr (std::is_same_v<T, core::Decimal>) {
        return core::Decimal{std::to_string(ordinal) + ".00"};
      } else if constexpr (std::is_same_v<T, core::Binary>) {
        return core::Binary{
          std::byte{static_cast<unsigned char>(ordinal & 0xffU)},
          std::byte{static_cast<unsigned char>((ordinal >> 8U) & 0xffU)},
        };
      } else if constexpr (std::is_same_v<T, std::chrono::sys_days>) {
        return std::chrono::sys_days{std::chrono::year{2000} / std::chrono::January / 1} +
               std::chrono::days{ordinal - 1};
      } else {
        return T{};
      }
    }

    [[nodiscard]]
    std::string generatedTextValue(core::ColumnTypeKind kind, std::size_t ordinal)
    {
      if (kind == core::ColumnTypeKind::Time) {
        const std::size_t seconds = (ordinal - 1) % (24 * 60 * 60);
        std::ostringstream value;
        value << std::setfill('0') << std::setw(2) << seconds / 3600 << ':' << std::setw(2) << (seconds / 60) % 60
              << ':' << std::setw(2) << seconds % 60;
        return value.str();
      }
      if (kind == core::ColumnTypeKind::DateTime) {
        std::ostringstream value;
        value << "2000-01-01 00:00:" << std::setfill('0') << std::setw(2) << (ordinal - 1) % 60;
        return value.str();
      }
      if (kind == core::ColumnTypeKind::Uuid) {
        std::ostringstream value;
        value << "00000000-0000-0000-0000-" << std::setfill('0') << std::setw(12) << ordinal;
        return value.str();
      }
      if (kind == core::ColumnTypeKind::Json) {
        return "{\"seed\":" + std::to_string(ordinal) + '}';
      }
      return "_test_" + std::to_string(ordinal);
    }

    [[nodiscard]]
    std::string generatedStringValue(const core::ColumnType& type, std::size_t ordinal)
    {
      if (!type.length.has_value()) {
        return "_test_" + std::to_string(ordinal);
      }

      constexpr std::string_view digits = "0123456789abcdefghijklmnopqrstuvwxyz";
      std::string value;
      std::size_t remaining = ordinal - 1;
      do {
        value.push_back(digits[remaining % digits.size()]);
        remaining /= digits.size();
      } while (remaining > 0);
      std::ranges::reverse(value);
      return value;
    }

    [[nodiscard]]
    core::Decimal generatedDecimalValue(const core::ColumnType& type, std::size_t ordinal)
    {
      const std::string digits = std::to_string(ordinal);
      const std::size_t scale = type.scale.value_or(0);
      if (type.precision.has_value() && scale == *type.precision) {
        std::string value{"."};
        if (digits.size() < scale) {
          value.append(scale - digits.size(), '0');
        }
        value += digits;
        return core::Decimal{value};
      }

      std::string value = digits;
      if (scale > 0) {
        value += '.';
        value.append(scale, '0');
      }
      return core::Decimal{value};
    }

    template <core::ColumnTypeKind Kind>
    PopulationPlanColumn generatePopulationPlanColumn(const core::SchemaColumnSnapshot& column, std::size_t ordinal)
    {
      using ValueType = core::ColumnType_t<Kind>;
      core::Parameter value;
      if constexpr (Kind == core::ColumnTypeKind::Enum) {
        if (column.type.enumeration.has_value() && !column.type.enumeration->values.empty()) {
          value = column.type.enumeration->values[(ordinal - 1) % column.type.enumeration->values.size()];
        } else {
          value = generatedTextValue(Kind, ordinal);
        }
      } else if constexpr (Kind == core::ColumnTypeKind::Decimal) {
        value = generatedDecimalValue(column.type, ordinal);
      } else if constexpr (Kind == core::ColumnTypeKind::Binary) {
        std::vector<std::byte> bytes{
          std::byte{static_cast<unsigned char>(ordinal & 0xffU)},
          std::byte{static_cast<unsigned char>((ordinal >> 8U) & 0xffU)},
        };
        if (column.type.length.has_value() && bytes.size() > *column.type.length) {
          bytes.resize(*column.type.length);
        }
        value = core::Binary{std::move(bytes)};
      } else if constexpr (Kind == core::ColumnTypeKind::String) {
        value = generatedStringValue(column.type, ordinal);
      } else if constexpr (Kind == core::ColumnTypeKind::Time || Kind == core::ColumnTypeKind::DateTime ||
                           Kind == core::ColumnTypeKind::Uuid || Kind == core::ColumnTypeKind::Json) {
        std::string text = generatedTextValue(Kind, ordinal);
        if (column.type.length.has_value() && text.size() > *column.type.length) {
          text.resize(*column.type.length);
        }
        value = std::move(text);
      } else {
        value = core::encode(generateColumnValue<ValueType>(ordinal));
      }

      return {
        .name = column.name,
        .type = column.type,
        .value = std::move(value),
      };
    }

    template <core::ColumnTypeKind Kind>
    [[nodiscard]]
    bool hasValidGeneratedType(const core::Parameter& value)
    {
      return std::holds_alternative<core::ColumnType_t<Kind>>(core::decode<core::ColumnType_t<Kind>>(value));
    }

    [[nodiscard]]
    bool hasValidGeneratedValue(const PopulationPlanColumn& column)
    {
      bool validType = false;
      switch (column.type.kind) {
      case core::ColumnTypeKind::Boolean:
        validType = hasValidGeneratedType<core::ColumnTypeKind::Boolean>(column.value);
        break;
      case core::ColumnTypeKind::Int16:
        validType = hasValidGeneratedType<core::ColumnTypeKind::Int16>(column.value);
        break;
      case core::ColumnTypeKind::Int32:
        validType = hasValidGeneratedType<core::ColumnTypeKind::Int32>(column.value);
        break;
      case core::ColumnTypeKind::Int64:
        validType = hasValidGeneratedType<core::ColumnTypeKind::Int64>(column.value);
        break;
      case core::ColumnTypeKind::Float32:
        validType = hasValidGeneratedType<core::ColumnTypeKind::Float32>(column.value);
        break;
      case core::ColumnTypeKind::Float64:
        validType = hasValidGeneratedType<core::ColumnTypeKind::Float64>(column.value);
        break;
      case core::ColumnTypeKind::Decimal:
        validType = hasValidGeneratedType<core::ColumnTypeKind::Decimal>(column.value);
        break;
      case core::ColumnTypeKind::String:
        validType = hasValidGeneratedType<core::ColumnTypeKind::String>(column.value);
        break;
      case core::ColumnTypeKind::Enum:
        validType = hasValidGeneratedType<core::ColumnTypeKind::Enum>(column.value);
        break;
      case core::ColumnTypeKind::Binary:
        validType = hasValidGeneratedType<core::ColumnTypeKind::Binary>(column.value);
        break;
      case core::ColumnTypeKind::Date:
        validType = hasValidGeneratedType<core::ColumnTypeKind::Date>(column.value);
        break;
      case core::ColumnTypeKind::Time:
        validType = hasValidGeneratedType<core::ColumnTypeKind::Time>(column.value);
        break;
      case core::ColumnTypeKind::DateTime:
        validType = hasValidGeneratedType<core::ColumnTypeKind::DateTime>(column.value);
        break;
      case core::ColumnTypeKind::Uuid:
        validType = hasValidGeneratedType<core::ColumnTypeKind::Uuid>(column.value);
        break;
      case core::ColumnTypeKind::Json:
        validType = hasValidGeneratedType<core::ColumnTypeKind::Json>(column.value);
        break;
      case core::ColumnTypeKind::Unknown:
        return false;
      }

      if (!validType) {
        return false;
      }

      if (column.type.kind == core::ColumnTypeKind::Decimal) {
        const core::Decimal& value = std::get<core::Decimal>(column.value);
        return (!column.type.precision.has_value() || value.precision() <= *column.type.precision) &&
               (!column.type.scale.has_value() || value.scale() <= *column.type.scale);
      }

      if (column.type.kind == core::ColumnTypeKind::Binary) {
        return !column.type.length.has_value() || std::get<core::Binary>(column.value).size() <= *column.type.length;
      }

      if (column.type.kind == core::ColumnTypeKind::String || column.type.kind == core::ColumnTypeKind::Enum) {
        const std::string& value = std::get<std::string>(column.value);
        if (column.type.length.has_value() && value.size() > *column.type.length) {
          return false;
        }
        if (column.type.kind == core::ColumnTypeKind::Enum && column.type.enumeration.has_value()) {
          return std::ranges::find(column.type.enumeration->values, value) != column.type.enumeration->values.end();
        }
      }

      return true;
    }

    void validateGeneratedValues(const PopulationPlan& plan)
    {
      for (const PopulationPlanTable& table : plan.tables) {
        for (const PopulationPlanRow& row : table.rows) {
          for (const PopulationPlanColumn& column : row.columns) {
            if (!hasValidGeneratedValue(column)) {
              throw InvalidCliArgumentException(
                "Generated seed value for column '{}.{}' is incompatible with its database type.",
                table.schema.empty() ? table.name : table.schema + "." + table.name,
                column.name);
            }
          }
        }
      }
    }

    [[nodiscard]]
    const PopulationPlanTable* findPlanTable(
      const PopulationPlan& plan,
      std::string_view schema,
      std::string_view name) noexcept
    {
      const auto table = std::ranges::find_if(plan.tables, [&](const PopulationPlanTable& candidate) {
        return candidate.schema == schema && candidate.name == name;
      });

      return table == plan.tables.end() ? nullptr : &*table;
    }

    [[nodiscard]]
    PopulationPlanTable* findPlanTable(PopulationPlan& plan, std::string_view schema, std::string_view name) noexcept
    {
      return const_cast<PopulationPlanTable*>(findPlanTable(std::as_const(plan), schema, name));
    }

    [[nodiscard]]
    const PopulationPlanColumn* findPlanColumn(const PopulationPlanRow& row, std::string_view name) noexcept
    {
      const auto column = std::ranges::find(row.columns, name, &PopulationPlanColumn::name);
      return column == row.columns.end() ? nullptr : &*column;
    }

    [[nodiscard]]
    PopulationPlanColumn* findPlanColumn(PopulationPlanRow& row, std::string_view name) noexcept
    {
      return const_cast<PopulationPlanColumn*>(findPlanColumn(std::as_const(row), name));
    }

    void validateTableConstraints(
      const PopulationPlan& plan,
      const std::vector<const core::SchemaTableSnapshot*>& tables)
    {
      for (const core::SchemaTableSnapshot* source : tables) {
        const PopulationPlanTable* table = findPlanTable(plan, source->schema, source->name);
        if (table == nullptr) {
          throw InvalidCliArgumentException("Seed plan is missing table '{}'.", tableLabel(*source));
        }

        for (const PopulationPlanRow& row : table->rows) {
          for (const core::SchemaColumnSnapshot& column : source->columns) {
            const PopulationPlanColumn* planned = findPlanColumn(row, column.name);
            if (planned == nullptr && !column.nullable && !column.defaultExpression.has_value() && !column.generated) {
              throw InvalidCliArgumentException(
                "Seed plan cannot provide a value for required column '{}.{}'.",
                tableLabel(*source),
                column.name);
            }

            if (planned != nullptr && std::holds_alternative<std::nullptr_t>(planned->value) && !column.nullable) {
              throw InvalidCliArgumentException(
                "Seed plan provides null for required column '{}.{}'.",
                tableLabel(*source),
                column.name);
            }
          }
        }

        for (const std::string& primaryKey : source->primaryKey) {
          const core::SchemaColumnSnapshot* column = source->findColumn(primaryKey);
          if (column == nullptr) {
            throw InvalidCliArgumentException(
              "Table '{}' has an invalid primary-key column '{}'.",
              tableLabel(*source),
              primaryKey);
          }

          for (const PopulationPlanRow& row : table->rows) {
            if (findPlanColumn(row, primaryKey) == nullptr && !column->defaultExpression.has_value() &&
                !column->generated) {
              throw InvalidCliArgumentException(
                "Seed plan is missing primary-key column '{}.{}'.",
                tableLabel(*source),
                primaryKey);
            }
          }
        }

        const auto validateUniqueValues = [&](const std::vector<std::string>& columns, std::string_view constraint) {
          std::unordered_set<core::Statement, core::StatementHash> values;
          values.reserve(table->rows.size());
          for (const PopulationPlanRow& row : table->rows) {
            std::vector<core::Parameter> parameters;
            parameters.reserve(columns.size());
            for (const std::string& columnName : columns) {
              const PopulationPlanColumn* column = findPlanColumn(row, columnName);
              if (column == nullptr) {
                parameters.clear();
                break;
              }
              parameters.push_back(column->value);
            }

            if (!parameters.empty() && !values.emplace(std::string{constraint}, std::move(parameters)).second) {
              throw InvalidCliArgumentException(
                "Seed plan generates duplicate values for {} on table '{}'.",
                constraint,
                tableLabel(*source));
            }
          }
        };

        if (!source->primaryKey.empty()) {
          validateUniqueValues(source->primaryKey, "primary key");
        }
        for (const core::SchemaColumnSnapshot& column : source->columns) {
          if (column.unique) {
            validateUniqueValues({column.name}, "unique column");
          }
        }
      }
    }

    void bindForeignKeyValues(PopulationPlan& plan, const std::vector<const core::SchemaTableSnapshot*>& tables)
    {
      for (const core::SchemaTableSnapshot* source : tables) {
        PopulationPlanTable* table = findPlanTable(plan, source->schema, source->name);
        for (const std::string& foreignKey : source->foreignKey) {
          const std::optional<ForeignKeyReference> reference = parseForeignKey(foreignKey, source->schema);
          if (!reference.has_value()) {
            throw InvalidCliArgumentException(
              "Table '{}' has an invalid foreign-key description '{}'.",
              tableLabel(*source),
              foreignKey);
          }

          const PopulationPlanTable* referencedTable =
            findPlanTable(plan, reference->table.schema, reference->table.table);
          if (referencedTable == nullptr || referencedTable->rows.empty()) {
            throw InvalidCliArgumentException(
              "Seed plan cannot bind foreign key '{}' on table '{}'.",
              foreignKey,
              tableLabel(*source));
          }

          for (std::size_t rowIndex = 0; rowIndex < table->rows.size(); ++rowIndex) {
            PopulationPlanColumn* column = findPlanColumn(table->rows[rowIndex], reference->column);
            const PopulationPlanRow& referencedRow = referencedTable->rows[rowIndex % referencedTable->rows.size()];
            const PopulationPlanColumn* referencedColumn = findPlanColumn(referencedRow, reference->referencedColumn);
            if (column == nullptr || referencedColumn == nullptr) {
              const auto referencedSchemaTable =
                std::ranges::find_if(tables, [&](const core::SchemaTableSnapshot* candidate) {
                  return candidate->schema == reference->table.schema && candidate->name == reference->table.table;
                });
              const core::SchemaColumnSnapshot* referencedSchemaColumn =
                referencedSchemaTable == tables.end()
                  ? nullptr
                  : (*referencedSchemaTable)->findColumn(reference->referencedColumn);
              if (referencedSchemaColumn != nullptr &&
                  (referencedSchemaColumn->generated || referencedSchemaColumn->defaultExpression.has_value())) {
                throw InvalidCliArgumentException(
                  "Seed plan cannot bind foreign key '{}' on table '{}' because referenced column '{}.{}' is "
                  "database-generated.",
                  foreignKey,
                  tableLabel(*source),
                  referencedTable->schema.empty() ? referencedTable->name
                                                  : referencedTable->schema + "." + referencedTable->name,
                  reference->referencedColumn);
              }
              throw InvalidCliArgumentException(
                "Seed plan cannot bind foreign key '{}' on table '{}'.",
                foreignKey,
                tableLabel(*source));
            }

            column->value = referencedColumn->value;
            if (!hasValidGeneratedValue(*column)) {
              throw InvalidCliArgumentException(
                "Foreign-key value for column '{}.{}' is incompatible with its database type.",
                tableLabel(*source),
                column->name);
            }
          }
        }
      }
    }

    [[nodiscard]]
    std::vector<const core::SchemaTableSnapshot*> mapTables(
      const Invocation& invocation,
      const core::SchemaSnapshot& databaseSchema)
    {
      if (invocation.arguments.all) {
        std::vector<const core::SchemaTableSnapshot*> selected;
        selected.reserve(databaseSchema.tables.size());
        for (const core::SchemaTableSnapshot& table : databaseSchema.tables) {
          selected.push_back(&table);
        }
        return selected;
      }

      std::vector<const core::SchemaTableSnapshot*> selected;
      selected.reserve(databaseSchema.tables.size());

      for (const core::SchemaTableSnapshot& table : databaseSchema.tables) {
        const bool validSchema =
          !invocation.arguments.schema.has_value() || table.schema == *invocation.arguments.schema;
        const bool validTable =
          invocation.arguments.tables.empty() ||
          std::ranges::find(invocation.arguments.tables, table.name) != invocation.arguments.tables.end();

        if (validSchema && validTable) {
          selected.push_back(&table);
        }
      }

      for (const std::string& requested : invocation.arguments.tables) {
        if (std::ranges::find(selected, requested, &core::SchemaTableSnapshot::name) == selected.end()) {
          throw InvalidCliArgumentException("Selected seed table '{}' was not found.", requested);
        }
      }

      return selected;
    }

    [[nodiscard]]
    std::vector<DependencyGraphNode> buildDependencyGraph(
      const std::vector<const core::SchemaTableSnapshot*>& selected,
      const core::SchemaSnapshot& databaseSchema)
    {
      std::vector<DependencyGraphNode> graph;
      std::unordered_map<std::string, std::size_t> nodeIndexes;

      const auto addTable = [&](const core::SchemaTableSnapshot* table) {
        const std::string key = tableKey(table->schema, table->name);
        const auto existing = nodeIndexes.find(key);
        if (existing != nodeIndexes.end()) {
          return existing->second;
        }

        const std::size_t index = graph.size();
        graph.push_back({.table = table});
        nodeIndexes.emplace(key, index);
        return index;
      };

      for (const core::SchemaTableSnapshot* table : selected) {
        addTable(table);
      }

      for (std::size_t nodeIndex = 0; nodeIndex < graph.size(); ++nodeIndex) {
        const core::SchemaTableSnapshot* table = graph[nodeIndex].table;
        for (const std::string& foreignKey : table->foreignKey) {
          const std::optional<TableReference> reference = referencedTable(foreignKey, table->schema);
          if (!reference.has_value() || reference->table.empty()) {
            throw InvalidCliArgumentException(
              "Table '{}' has an invalid foreign-key description '{}'.",
              tableLabel(*table),
              foreignKey);
          }

          const core::SchemaTableSnapshot* dependency = databaseSchema.findTable(reference->schema, reference->table);
          if (dependency == nullptr) {
            throw InvalidCliArgumentException(
              "Table '{}' references missing table '{}'.",
              tableLabel(*table),
              reference->schema.empty() ? reference->table : reference->schema + "." + reference->table);
          }

          const std::size_t dependencyIndex = addTable(dependency);
          if (std::ranges::find(graph[nodeIndex].dependencies, dependencyIndex) ==
              graph[nodeIndex].dependencies.end()) {
            graph[nodeIndex].dependencies.push_back(dependencyIndex);
          }
        }
      }

      return graph;
    }

    [[nodiscard]]
    std::vector<const core::SchemaTableSnapshot*> orderDependencies(const std::vector<DependencyGraphNode>& graph)
    {
      std::vector<std::size_t> remainingDependencies;
      std::vector<std::vector<std::size_t>> dependants(graph.size());
      remainingDependencies.reserve(graph.size());

      for (std::size_t nodeIndex = 0; nodeIndex < graph.size(); ++nodeIndex) {
        remainingDependencies.push_back(graph[nodeIndex].dependencies.size());
        for (const std::size_t dependency : graph[nodeIndex].dependencies) {
          dependants[dependency].push_back(nodeIndex);
        }
      }

      std::vector<std::size_t> ready;
      for (std::size_t nodeIndex = 0; nodeIndex < graph.size(); ++nodeIndex) {
        if (remainingDependencies[nodeIndex] == 0) {
          ready.push_back(nodeIndex);
        }
      }

      std::vector<const core::SchemaTableSnapshot*> ordered;
      ordered.reserve(graph.size());
      for (std::size_t readyIndex = 0; readyIndex < ready.size(); ++readyIndex) {
        const std::size_t nodeIndex = ready[readyIndex];
        ordered.push_back(graph[nodeIndex].table);
        for (const std::size_t dependant : dependants[nodeIndex]) {
          if (--remainingDependencies[dependant] == 0) {
            ready.push_back(dependant);
          }
        }
      }

      if (ordered.size() != graph.size()) {
        throw InvalidCliArgumentException("Selected seed tables contain a foreign-key dependency cycle.");
      }

      return ordered;
    }

    [[nodiscard]]
    std::size_t requestedRowCount(const Invocation& invocation)
    {
      if (!invocation.arguments.rows.has_value()) {
        return 1;
      }

      std::size_t rows = 0;
      const std::string_view value = *invocation.arguments.rows;
      const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), rows);
      if (error != std::errc{} || end != value.data() + value.size() || rows == 0) {
        throw InvalidCliArgumentException("Option '--rows' must be a positive integer.");
      }
      return rows;
    }

    [[nodiscard]]
    PopulationPlan buildPopulationPlan(
      const std::vector<const core::SchemaTableSnapshot*>& tables,
      const std::vector<const core::SchemaTableSnapshot*>& selected,
      std::size_t requestedRows,
      const std::shared_ptr<SeedMetrics>& metrics)
    {
      const auto generationStarted = Clock::now();
      PopulationPlan plan;
      plan.tables.reserve(tables.size());

      std::unordered_map<std::string, bool> selectedTables;
      selectedTables.reserve(selected.size());
      for (const core::SchemaTableSnapshot* table : selected) {
        selectedTables.emplace(tableKey(table->schema, table->name), true);
      }

      for (const core::SchemaTableSnapshot* source : tables) {
        PopulationPlanTable table{.schema = source->schema, .name = source->name};
        const bool explicitlySelected = selectedTables.contains(tableKey(source->schema, source->name));
        const std::size_t rowCount = explicitlySelected ? requestedRows : 1;
        table.rows.reserve(rowCount);

        for (std::size_t rowIndex = 0; rowIndex < rowCount; ++rowIndex) {
          const std::size_t ordinal = rowIndex + 1;
          PopulationPlanRow row;
          row.columns.reserve(source->columns.size());

          for (const core::SchemaColumnSnapshot& column : source->columns) {
            if (column.defaultExpression.has_value() || column.generated) {
              ++metrics->databaseGeneratedValues;
              continue;
            }

            const auto append = [&]<core::ColumnTypeKind Kind>() {
              row.columns.push_back(generatePopulationPlanColumn<Kind>(column, ordinal));
              ++metrics->generatedValues;
            };
            switch (column.type.kind) {
            case core::ColumnTypeKind::Boolean:
              append.template operator()<core::ColumnTypeKind::Boolean>();
              break;
            case core::ColumnTypeKind::Int16:
              append.template operator()<core::ColumnTypeKind::Int16>();
              break;
            case core::ColumnTypeKind::Int32:
              append.template operator()<core::ColumnTypeKind::Int32>();
              break;
            case core::ColumnTypeKind::Int64:
              append.template operator()<core::ColumnTypeKind::Int64>();
              break;
            case core::ColumnTypeKind::Float32:
              append.template operator()<core::ColumnTypeKind::Float32>();
              break;
            case core::ColumnTypeKind::Float64:
              append.template operator()<core::ColumnTypeKind::Float64>();
              break;
            case core::ColumnTypeKind::Decimal:
              append.template operator()<core::ColumnTypeKind::Decimal>();
              break;
            case core::ColumnTypeKind::String:
              append.template operator()<core::ColumnTypeKind::String>();
              break;
            case core::ColumnTypeKind::Enum:
              append.template operator()<core::ColumnTypeKind::Enum>();
              break;
            case core::ColumnTypeKind::Binary:
              append.template operator()<core::ColumnTypeKind::Binary>();
              break;
            case core::ColumnTypeKind::Date:
              append.template operator()<core::ColumnTypeKind::Date>();
              break;
            case core::ColumnTypeKind::Time:
              append.template operator()<core::ColumnTypeKind::Time>();
              break;
            case core::ColumnTypeKind::DateTime:
              append.template operator()<core::ColumnTypeKind::DateTime>();
              break;
            case core::ColumnTypeKind::Uuid:
              append.template operator()<core::ColumnTypeKind::Uuid>();
              break;
            case core::ColumnTypeKind::Json:
              append.template operator()<core::ColumnTypeKind::Json>();
              break;
            case core::ColumnTypeKind::Unknown:
              continue;
            }
          }

          table.rows.push_back(std::move(row));
        }

        metrics->plannedRows += rowCount;
        if (!explicitlySelected) {
          metrics->dependencyRows += rowCount;
        }
        plan.tables.push_back(std::move(table));
      }

      metrics->generationDuration = Clock::now() - generationStarted;
      return plan;
    }

    [[nodiscard]]
    std::string executePopulationPlan(
      const Invocation& invocation,
      const PopulationPlan& plan,
      const std::shared_ptr<SeedMetrics>& metrics)
    try {
      const auto mapColumns =
        [](const std::vector<PopulationPlanColumn>& columns) -> std::vector<std::pair<std::string, core::Parameter>> {
        std::vector<std::pair<std::string, core::Parameter>> mapped;
        mapped.reserve(columns.size());
        for (const PopulationPlanColumn& column : columns) {
          mapped.emplace_back(column.name, column.value);
        }
        return mapped;
      };

      const connection::DatabaseType type = databaseType(invocation);
      const std::shared_ptr<connection::Client> client =
        DependencyInjector<connection::Client>::get(connectionConfig(invocation, type), type);
      const core::QueryBuilder queryBuilder = DependencyInjector<core::QueryBuilder>::get(type);
      const core::Repository<SeedRecord> repository{client, queryBuilder};
      std::vector<core::Statement> statements;
      statements.reserve(metrics->plannedRows);
      for (const PopulationPlanTable& table : plan.tables) {
        const std::string tableName = table.schema.empty() ? table.name : table.schema + "." + table.name;
        for (const PopulationPlanRow& row : table.rows) {
          if (row.columns.empty()) {
            statements.emplace_back(
              type == connection::DatabaseType::MySQL ? "insert into " + tableName + " () values ()"
                                                      : "insert into " + tableName + " default values");
          } else {
            statements.push_back(queryBuilder.insert(core::Source{tableName}, mapColumns(row.columns)));
          }
        }
      }
      metrics->generatedStatements = statements.size();

      auto transaction = client->beginTransaction();
      for (const core::Statement& statement : statements) {
        metrics->insertedRows += repository.insert(statement);
        ++metrics->executedStatements;
      }
      transaction.commit();
      return {};
    } catch (const std::exception& error) {
      metrics->insertedRows = 0;
      return error.what();
    }

    ExecutionReport populate(
      const Invocation& invocation,
      const core::SchemaSnapshot& databaseSchema,
      const std::shared_ptr<SeedMetrics>& metrics)
    {
      const auto planningStarted = Clock::now();
      const std::vector<const core::SchemaTableSnapshot*> selected = mapTables(invocation, databaseSchema);
      const std::vector<DependencyGraphNode> dependencyGraph = buildDependencyGraph(selected, databaseSchema);
      const std::vector<const core::SchemaTableSnapshot*> orderedTables = orderDependencies(dependencyGraph);
      const std::size_t requestedRows = requestedRowCount(invocation);
      PopulationPlan plan = buildPopulationPlan(orderedTables, selected, requestedRows, metrics);

      metrics->planningDuration = Clock::now() - planningStarted;
      metrics->tablesDiscovered = databaseSchema.tables.size();
      metrics->tablesSelected = selected.size();
      metrics->dependencyTables = orderedTables.size() - selected.size();
      metrics->requestedRows = requestedRows;

      validateGeneratedValues(plan);
      bindForeignKeyValues(plan, orderedTables);
      validateTableConstraints(plan, orderedTables);

      std::string executionError;
      if (invocation.arguments.apply) {
        const auto executionStarted = Clock::now();
        executionError = executePopulationPlan(invocation, plan, metrics);
        metrics->executionDuration = Clock::now() - executionStarted;
      }

      const bool success = executionError.empty();

      return {
        .info = success ? "Seed completed successfully." : "Seed failed: " + executionError,
        .status = success ? ExecutionStatus::Success : ExecutionStatus::Failed,
        .metrics = metrics,
      };
    }
  } // namespace

  void SeedMetrics::writeText(std::ostream& out) const
  {
    out << "Seed summary\n\n";
    out << "Discovery:\n";
    printMetric(out, "Tables discovered", tablesDiscovered);
    printMetric(out, "Tables selected", tablesSelected);
    printMetric(out, "Dependency tables", dependencyTables);
    out << "\nPlan:\n";
    printMetric(out, "Requested rows", requestedRows);
    printMetric(out, "Planned rows", plannedRows);
    printMetric(out, "Dependency rows", dependencyRows);
    out << "\nValues:\n";
    printMetric(out, "Generated values", generatedValues);
    printMetric(out, "Provided values", providedValues);
    printMetric(out, "Database-generated", databaseGeneratedValues);
    out << "\nExecution:\n";
    printMetric(out, "Generated statements", generatedStatements);
    printMetric(out, "Executed statements", executedStatements);
    printMetric(out, "Inserted rows", insertedRows);
    out << "\nTiming:\n";
    printDuration(out, "Introspection", introspectionDuration);
    printDuration(out, "Planning", planningDuration);
    printDuration(out, "Generation", generationDuration);
    printDuration(out, "Execution", executionDuration);
    printDuration(out, "Total", totalDuration);
  }

  void SeedMetrics::writeJson(std::ostream& out) const
  {
    out << "{\"tablesDiscovered\":" << tablesDiscovered << ",\"tablesSelected\":" << tablesSelected
        << ",\"dependencyTables\":" << dependencyTables << ",\"requestedRows\":" << requestedRows
        << ",\"plannedRows\":" << plannedRows << ",\"dependencyRows\":" << dependencyRows
        << ",\"generatedValues\":" << generatedValues << ",\"providedValues\":" << providedValues
        << ",\"databaseGeneratedValues\":" << databaseGeneratedValues
        << ",\"generatedStatements\":" << generatedStatements << ",\"executedStatements\":" << executedStatements
        << ",\"insertedRows\":" << insertedRows
        << ",\"introspectionDurationMs\":" << milliseconds(introspectionDuration)
        << ",\"planningDurationMs\":" << milliseconds(planningDuration)
        << ",\"generationDurationMs\":" << milliseconds(generationDuration)
        << ",\"executionDurationMs\":" << milliseconds(executionDuration)
        << ",\"totalDurationMs\":" << milliseconds(totalDuration) << '}';
  }

  ExecutionReport seed(const Invocation& invocation, const core::SchemaSnapshot& databaseSchema)
  {
    const auto started = Clock::now();
    auto metrics = std::make_shared<SeedMetrics>();
    ExecutionReport report = populate(invocation, databaseSchema, metrics);
    metrics->totalDuration = Clock::now() - started;
    return report;
  }

  ExecutionReport seed(const Invocation& invocation)
  {
    const auto started = Clock::now();
    auto inspector = schemaInspector(invocation);
    const core::SchemaSnapshot databaseSchema = inspector.inspect();
    const auto discoveryFinished = Clock::now();
    auto metrics = std::make_shared<SeedMetrics>();
    ExecutionReport report = populate(invocation, databaseSchema, metrics);
    metrics->introspectionDuration = discoveryFinished - started;
    metrics->totalDuration = Clock::now() - started;
    return report;
  }
} // namespace worm::cli::database
