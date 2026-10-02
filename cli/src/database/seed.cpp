#include "seed.hpp"

#include <core/model/column.hpp>
#include <core/model/schema.hpp>
#include <core/query/expression.hpp>
#include <errors/invalid-cli-argument-exception.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <helpers/connection.hpp>

namespace worm::cli::database
{
  namespace
  {
    using Clock = std::chrono::steady_clock;

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

    struct PopulationPlanTable
    {
      std::string schema;
      std::string name;
      std::vector<PopulationPlanColumn> columns;
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
    std::optional<ForeignKeyReference> parseForeignKey(
      std::string_view foreignKey,
      std::string_view defaultSchema)
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
    T generateColumnValue()
    {
      if constexpr (std::is_same_v<T, bool>) {
        return true;
      } else if constexpr (std::is_integral_v<T>) {
        return static_cast<T>(1);
      } else if constexpr (std::is_floating_point_v<T>) {
        return static_cast<T>(1.0);
      } else if constexpr (utils::is_string_like<T>) {
        return "_test";
      } else if constexpr (std::is_same_v<T, core::Decimal>) {
        return core::Decimal{"1.00"};
      } else if constexpr (std::is_same_v<T, core::Binary>) {
        return core::Binary{std::byte{0x00}, std::byte{0x7f}, std::byte{0xff}};
      } else if constexpr (std::is_same_v<T, std::chrono::sys_days>) {
        return std::chrono::sys_days{std::chrono::year{2000} / std::chrono::January / 1};
      } else {
        return T{};
      }
    }

    template <core::ColumnTypeKind Kind>
    PopulationPlanColumn generatePopulationPlanColumn(const core::SchemaColumnSnapshot& column)
    {
      using ValueType = core::ColumnType_t<Kind>;
      return {
        .name = column.name,
        .type = column.type,
        .value = core::encode(generateColumnValue<ValueType>()),
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
        for (const PopulationPlanColumn& column : table.columns) {
          if (!hasValidGeneratedValue(column)) {
            throw InvalidCliArgumentException(
              "Generated seed value for column '{}.{}' is incompatible with its database type.",
              table.schema.empty() ? table.name : table.schema + "." + table.name,
              column.name);
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
    PopulationPlanTable* findPlanTable(
      PopulationPlan& plan,
      std::string_view schema,
      std::string_view name) noexcept
    {
      return const_cast<PopulationPlanTable*>(findPlanTable(std::as_const(plan), schema, name));
    }

    [[nodiscard]]
    const PopulationPlanColumn* findPlanColumn(const PopulationPlanTable& table, std::string_view name) noexcept
    {
      const auto column = std::ranges::find(table.columns, name, &PopulationPlanColumn::name);
      return column == table.columns.end() ? nullptr : &*column;
    }

    [[nodiscard]]
    PopulationPlanColumn* findPlanColumn(PopulationPlanTable& table, std::string_view name) noexcept
    {
      return const_cast<PopulationPlanColumn*>(findPlanColumn(std::as_const(table), name));
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

        for (const core::SchemaColumnSnapshot& column : source->columns) {
          const PopulationPlanColumn* planned = findPlanColumn(*table, column.name);
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

        for (const std::string& primaryKey : source->primaryKey) {
          const core::SchemaColumnSnapshot* column = source->findColumn(primaryKey);
          if (column == nullptr) {
            throw InvalidCliArgumentException(
              "Table '{}' has an invalid primary-key column '{}'.",
              tableLabel(*source),
              primaryKey);
          }

          if (findPlanColumn(*table, primaryKey) == nullptr &&
              !column->defaultExpression.has_value() &&
              !column->generated) {
            throw InvalidCliArgumentException(
              "Seed plan is missing primary-key column '{}.{}'.",
              tableLabel(*source),
              primaryKey);
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

          PopulationPlanColumn* column = findPlanColumn(*table, reference->column);
          const PopulationPlanTable* referencedTable =
            findPlanTable(plan, reference->table.schema, reference->table.table);
          const PopulationPlanColumn* referencedColumn =
            referencedTable == nullptr ? nullptr : findPlanColumn(*referencedTable, reference->referencedColumn);
          if (column == nullptr || referencedColumn == nullptr) {
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
          !invocation.arguments.schema.has_value() ||
          table.schema == *invocation.arguments.schema;
        const bool validTable =
          invocation.arguments.tables.empty() ||
          std::ranges::find(invocation.arguments.tables, table.name) != invocation.arguments.tables.end();

        if (validSchema || validTable) {
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
    PopulationPlan buildPopulationPlan(
      const std::vector<const core::SchemaTableSnapshot*>& tables,
      const std::shared_ptr<SeedMetrics>& metrics)
    {
      const auto generationStarted = Clock::now();
      PopulationPlan plan;
      plan.tables.reserve(tables.size());

      for (const core::SchemaTableSnapshot* source : tables) {
        PopulationPlanTable table{.schema = source->schema, .name = source->name};
        table.columns.reserve(source->columns.size());

        for (const core::SchemaColumnSnapshot& column : source->columns) {
          if (column.defaultExpression.has_value() || column.generated) {
            ++metrics->databaseGeneratedValues;
            continue;
          }

          switch (column.type.kind) {
          case core::ColumnTypeKind::Boolean:
            table.columns.push_back(generatePopulationPlanColumn<core::ColumnTypeKind::Boolean>(column));
            break;
          case core::ColumnTypeKind::Int16:
            table.columns.push_back(generatePopulationPlanColumn<core::ColumnTypeKind::Int16>(column));
            break;
          case core::ColumnTypeKind::Int32:
            table.columns.push_back(generatePopulationPlanColumn<core::ColumnTypeKind::Int32>(column));
            break;
          case core::ColumnTypeKind::Int64:
            table.columns.push_back(generatePopulationPlanColumn<core::ColumnTypeKind::Int64>(column));
            break;
          case core::ColumnTypeKind::Float32:
            table.columns.push_back(generatePopulationPlanColumn<core::ColumnTypeKind::Float32>(column));
            break;
          case core::ColumnTypeKind::Float64:
            table.columns.push_back(generatePopulationPlanColumn<core::ColumnTypeKind::Float64>(column));
            break;
          case core::ColumnTypeKind::Decimal:
            table.columns.push_back(generatePopulationPlanColumn<core::ColumnTypeKind::Decimal>(column));
            break;
          case core::ColumnTypeKind::String:
            table.columns.push_back(generatePopulationPlanColumn<core::ColumnTypeKind::String>(column));
            break;
          case core::ColumnTypeKind::Enum:
            table.columns.push_back(generatePopulationPlanColumn<core::ColumnTypeKind::Enum>(column));
            break;
          case core::ColumnTypeKind::Binary:
            table.columns.push_back(generatePopulationPlanColumn<core::ColumnTypeKind::Binary>(column));
            break;
          case core::ColumnTypeKind::Date:
            table.columns.push_back(generatePopulationPlanColumn<core::ColumnTypeKind::Date>(column));
            break;
          case core::ColumnTypeKind::Time:
            table.columns.push_back(generatePopulationPlanColumn<core::ColumnTypeKind::Time>(column));
            break;
          case core::ColumnTypeKind::DateTime:
            table.columns.push_back(generatePopulationPlanColumn<core::ColumnTypeKind::DateTime>(column));
            break;
          case core::ColumnTypeKind::Uuid:
            table.columns.push_back(generatePopulationPlanColumn<core::ColumnTypeKind::Uuid>(column));
            break;
          case core::ColumnTypeKind::Json:
            table.columns.push_back(generatePopulationPlanColumn<core::ColumnTypeKind::Json>(column));
            break;
          case core::ColumnTypeKind::Unknown:
            continue;
          }
        }

        ++metrics->generatedValues;
        plan.tables.push_back(std::move(table));
      }

      metrics->generationDuration = Clock::now() - generationStarted;
      return plan;
    }

    [[nodiscard]]
    bool executePopulationPlan()
    try {
      // TODO: Execute the plan in a single transaction.
      return true;
    } catch (const std::exception& ex) {
      return false;
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
      PopulationPlan plan = buildPopulationPlan(orderedTables, metrics);

      metrics->planningDuration = Clock::now() - planningStarted;
      metrics->tablesDiscovered = databaseSchema.tables.size();
      metrics->tablesSelected = selected.size();
      metrics->dependencyTables = orderedTables.size();

      validateGeneratedValues(plan);
      validateTableConstraints(plan, orderedTables);
      bindForeignKeyValues(plan, orderedTables);
      // TODO: Update execution metrics.

      bool success = true;
      if (invocation.arguments.apply) {
        success = executePopulationPlan();
      }

      return {
        .info = success ? "Seed completed successfully." : "Seed failed.",
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
