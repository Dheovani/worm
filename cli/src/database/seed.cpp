#include "seed.hpp"

#include <core/model/column.hpp>
#include <core/model/schema.hpp>
#include <utils/helpers.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>
#include <typeindex>
#include <unordered_map>

#include <helpers/connection.hpp>
#include <helpers/file.hpp>

namespace worm::cli::database
{
  namespace
  {
    using Clock = std::chrono::steady_clock;

    struct DependencyGraphNode
    {
      std::string table;
      std::vector<std::string> dependencies;
    };

    template <typename T>
    struct PopulationPlanColumn
    {
      std::string name;
      core::ColumnType type;
      T value;
    };

    struct PopulationPlanTable
    {
      std::string name;
      std::unordered_map<std::type_index, std::shared_ptr<void>> columns;
    };

    struct PopulationPlan
    {
      std::vector<PopulationPlanTable> tables;
    };

    template <typename T>
    T generateColumnValue(const core::SchemaColumnSnapshot& column)
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
        return std::chrono::floor<std::chrono::days>(std::chrono::system_clock::from_time_t(t));
      } else {
        return T{};
      }
    }

    template <core::ColumnTypeKind Kind>
    std::shared_ptr<void> generatePopulationPlanColumn(const core::SchemaColumnSnapshot& column)
    {
      using ValueType = core::ColumnType_t<Kind>;

      return std::make_shared<PopulationPlanColumn<ValueType>>(
        PopulationPlanColumn<ValueType>{
          .name = column.name,
          .type = column.type,
          .value = generateColumnValue<ValueType>(column)
        });
    }

    PopulationPlan buildPopulationPlan(
      const std::vector<DependencyGraphNode>& dependencies,
      const core::SchemaSnapshot& databaseSchema)
    {
      PopulationPlan plan;

      for (const auto& node : dependencies) {
        PopulationPlanTable table;
        table.name = node.table;

        for (const auto& dep : node.dependencies) {
          auto depTable = databaseSchema.findTable("", dep);
          if (depTable == nullptr) {
            continue;
          }

          for (const auto& column : depTable->columns) {
            if (column.defaultExpression.has_value() || column.generated) {
              continue;
            }

            std::shared_ptr<void> planColumn;
            switch (column.type.kind) {
              case core::ColumnTypeKind::Boolean:
                planColumn = generatePopulationPlanColumn<core::ColumnTypeKind::Boolean>(column);
                break;

              case core::ColumnTypeKind::Int16:
                planColumn = generatePopulationPlanColumn<core::ColumnTypeKind::Int16>(column);
                break;

              case core::ColumnTypeKind::Int32:
                planColumn = generatePopulationPlanColumn<core::ColumnTypeKind::Int32>(column);
                break;

              case core::ColumnTypeKind::Int64:
                planColumn = generatePopulationPlanColumn<core::ColumnTypeKind::Int64>(column);
                break;

              case core::ColumnTypeKind::Float32:
                planColumn = generatePopulationPlanColumn<core::ColumnTypeKind::Float32>(column);
                break;

              case core::ColumnTypeKind::Float64:
                planColumn = generatePopulationPlanColumn<core::ColumnTypeKind::Float64>(column);
                break;

              case core::ColumnTypeKind::Decimal:
                planColumn = generatePopulationPlanColumn<core::ColumnTypeKind::Decimal>(column);
                break;

              case core::ColumnTypeKind::String:
                planColumn = generatePopulationPlanColumn<core::ColumnTypeKind::String>(column);
                break;

              case core::ColumnTypeKind::Enum:
                planColumn = generatePopulationPlanColumn<core::ColumnTypeKind::Enum>(column);
                break;

              case core::ColumnTypeKind::Binary:
                planColumn = generatePopulationPlanColumn<core::ColumnTypeKind::Binary>(column);
                break;

              case core::ColumnTypeKind::Date:
                planColumn = generatePopulationPlanColumn<core::ColumnTypeKind::Date>(column);
                break;

              case core::ColumnTypeKind::Time:
                planColumn = generatePopulationPlanColumn<core::ColumnTypeKind::Time>(column);
                break;

              case core::ColumnTypeKind::DateTime:
                planColumn = generatePopulationPlanColumn<core::ColumnTypeKind::DateTime>(column);
                break;

              case core::ColumnTypeKind::Uuid:
                planColumn = generatePopulationPlanColumn<core::ColumnTypeKind::Uuid>(column);
                break;

              case core::ColumnTypeKind::Json:
                planColumn = generatePopulationPlanColumn<core::ColumnTypeKind::Json>(column);
                break;

              case core::ColumnTypeKind::Unknown:
                continue;
            }

            table.columns.emplace(column.name, std::move(planColumn));
          }
        }

        plan.tables.push_back(std::move(table));
      }

      return plan;
    }

    std::vector<core::SchemaTableSnapshot> filterTables(
      std::vector<core::SchemaTableSnapshot> tables,
      const std::vector<std::string>& requestedTables)
    {
      if (requestedTables.empty()) {
        return tables;
      }

      std::vector<core::SchemaTableSnapshot> selected;
      for (const auto& table : tables) {
        if (std::ranges::find(requestedTables, table.name) != requestedTables.end()) {
          selected.push_back(table);
        }
      }

      return selected;
    }

    std::vector<core::SchemaTableSnapshot> filterTablesBySchema(
      std::vector<core::SchemaTableSnapshot> tables,
      const std::string schemaName)
    {
      std::vector<core::SchemaTableSnapshot> selected;
      for (const auto& table : tables) {
        if (table.schema == schemaName) {
          selected.push_back(table);
        }
      }

      return selected;
    }

    bool hasDependency(std::vector<DependencyGraphNode>& dependencyGraph, const std::string& foreignKey)
    {
      return std::ranges::find_if(
        dependencyGraph,
        [&](const DependencyGraphNode& node) {
          return node.table == foreignKey;
        }) == dependencyGraph.end();
    }

    ExecutionReport populate(
      const Invocation& invocation,
      const core::SchemaSnapshot& databaseSchema,
      const std::shared_ptr<SeedMetrics>& metrics)
    {
      // Mapear as tabelas e filtrar de acordo com os argumentos
      std::vector<core::SchemaTableSnapshot> tables = databaseSchema.tables;
      if (!invocation.arguments.all) {
        if (!invocation.arguments.tables.empty()) {
          tables = filterTables(tables, invocation.arguments.tables);
        }

        if (invocation.arguments.schema.has_value()) {
          tables = filterTablesBySchema(tables, invocation.arguments.schema.value());
        }
      }

      // Busca por dependências de tabelas e adiciona as dependências ao fim da lista de tabelas mapeadas
      std::vector<DependencyGraphNode> dependencyGraph;
      for (const auto& table : tables) {
        dependencyGraph.push_back({table.name, table.foreignKey});
        for (const auto& foreignKey : table.foreignKey) {
          if (hasDependency(dependencyGraph, foreignKey)) {
            auto depTable = databaseSchema.findTable(table.schema, foreignKey);
            if (depTable != nullptr) {
              tables.push_back(*depTable);
              dependencyGraph.push_back({depTable->name, depTable->foreignKey});
            }
          }
        }
      }

      // Organiza o grafo de dependências de acordo com a quantidade de dependências
      std::sort(
        dependencyGraph.begin(),
        dependencyGraph.end(),
        [](const DependencyGraphNode& a, const DependencyGraphNode& b) {
          return a.dependencies.size() < b.dependencies.size();
        });

      // Monta o plano de população de dados com base no grafo de dependências e no schema do banco de dados
      PopulationPlan plan = buildPopulationPlan(dependencyGraph, databaseSchema);
      // TODO: Validar se os valores gerados são válidos para cada coluna.
      // TODO: Validar se as constraints de cada tabela são respeitadas.
      // TODO: Validar se os relacionamentos entre tabelas são respeitados.
      // TODO: Executar tudo em uma única transação.
      // TODO: Atualizar as métricas de execução.
      // TODO: Retornar o relatório de execução com os dados atualizados.
      return {
        .info = "",
        .status = ExecutionStatus::Failed,
        .metrics = metrics,
      };
    }
  } // namespace

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
    const auto databaseSchema = inspector.inspect();
    const auto discoveryFinished = Clock::now();
    auto metrics = std::make_shared<SeedMetrics>();
    ExecutionReport report = populate(invocation, databaseSchema, metrics);
    metrics->introspectionDuration = discoveryFinished - started;
    metrics->totalDuration = Clock::now() - started;
    return report;
  }
} // namespace worm::cli::database
