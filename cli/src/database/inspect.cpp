#include "inspect.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <helpers/connection.hpp>

namespace worm::cli::database
{
  namespace
  {
    using Clock = std::chrono::steady_clock;

    struct SchemaInformation
    {
      std::string_view name;
      std::vector<const core::SchemaTableSnapshot*> tables;
    };

    [[nodiscard]]
    std::vector<SchemaInformation>
    extractSchemas(const core::SchemaSnapshot& databaseSchema, const std::shared_ptr<InspectMetrics>& metrics)
    {
      std::set<std::string_view> schemaNames;
      for (const auto& table : databaseSchema.tables) {
        schemaNames.insert(table.schema);
      }

      std::vector<SchemaInformation> schemas;
      schemas.reserve(schemaNames.size());
      for (const std::string_view schemaName : schemaNames) {
        schemas.push_back({.name = schemaName});
      }

      metrics->schemasDiscovered = schemas.size();
      metrics->tablesDiscovered = databaseSchema.tables.size();
      for (const auto& table : databaseSchema.tables) {
        metrics->columnsDiscovered += table.columns.size();
        metrics->primaryKeysDiscovered += table.primaryKey.size();
        metrics->foreignKeysDiscovered += table.foreignKey.size();
        metrics->indexesDiscovered += table.indexes.size();

        const auto schema = std::ranges::find(schemas, table.schema, &SchemaInformation::name);
        schema->tables.push_back(&table);
      }

      for (auto& schema : schemas) {
        std::ranges::sort(schema.tables, {}, &core::SchemaTableSnapshot::name);
      }

      return schemas;
    }

    [[nodiscard]]
    std::string columnTypeName(const core::ColumnType& type)
    {
      std::string name = type.nativeName.empty() ? std::string{core::columnTypeKindName(type.kind)} : type.nativeName;
      if (name.find('(') == std::string::npos) {
        if (type.length.has_value()) {
          name += '(' + std::to_string(*type.length) + ')';
        } else if (type.precision.has_value()) {
          name += '(' + std::to_string(*type.precision);
          if (type.scale.has_value()) {
            name += ',' + std::to_string(*type.scale);
          }
          name += ')';
        }
      }

      if (type.unsignedValue && name.find("unsigned") == std::string::npos) {
        name += " unsigned";
      }

      if (type.withTimeZone && name.find("time zone") == std::string::npos) {
        name += " with time zone";
      }

      return name;
    }

    void writeList(std::ostream& out, const std::vector<std::string>& values)
    {
      for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
          out << ", ";
        }
        out << values[index];
      }
    }

    [[nodiscard]]
    std::string translateText(const std::vector<SchemaInformation>& schemas)
    {
      std::ostringstream out;
      for (std::size_t schemaIndex = 0; schemaIndex < schemas.size(); ++schemaIndex) {
        const auto& schema = schemas[schemaIndex];
        if (schemaIndex != 0) {
          out << '\n';
        }
        out << "Schema: " << (schema.name.empty() ? "<default>" : schema.name) << '\n';

        for (const core::SchemaTableSnapshot* table : schema.tables) {
          out << "\n  Table: " << table->name << "\n    Columns:\n";

          std::size_t nameWidth = 0;
          std::size_t typeWidth = 0;
          std::vector<std::string> typeNames;
          typeNames.reserve(table->columns.size());
          for (const auto& column : table->columns) {
            nameWidth = (std::max)(nameWidth, column.name.size());
            typeNames.push_back(columnTypeName(column.type));
            typeWidth = (std::max)(typeWidth, typeNames.back().size());
          }

          for (std::size_t index = 0; index < table->columns.size(); ++index) {
            const auto& column = table->columns[index];
            out << "      " << std::left << std::setw(static_cast<int>(nameWidth + 2)) << column.name
                << std::setw(static_cast<int>(typeWidth + 2)) << typeNames[index] << std::right;

            if (!column.nullable) {
              out << "NOT NULL";
            }

            if (column.generated) {
              out << (column.nullable ? "GENERATED" : " GENERATED");
            }

            if (column.unique) {
              out << (column.nullable && !column.generated ? "UNIQUE" : " UNIQUE");
            }

            out << '\n';
          }

          if (!table->primaryKey.empty()) {
            out << "\n    Primary key:\n      ";
            writeList(out, table->primaryKey);
            out << '\n';
          }

          if (!table->foreignKey.empty()) {
            out << "\n    Foreign keys:\n";
            for (const auto& foreignKey : table->foreignKey) {
              out << "      " << foreignKey << '\n';
            }
          }

          if (!table->indexes.empty()) {
            out << "\n    Indexes:\n";
            for (const auto& index : table->indexes) {
              out << "      " << index << '\n';
            }
          }
        }
      }

      return std::move(out).str();
    }

    void writeJsonString(std::ostream& out, std::string_view value)
    {
      ExecutionMetrics::writeJsonString(out, value);
    }

    void writeJsonStringArray(std::ostream& out, const std::vector<std::string>& values)
    {
      out << '[';
      for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
          out << ',';
        }
        writeJsonString(out, values[index]);
      }
      out << ']';
    }

    [[nodiscard]]
    std::string translateJson(const std::vector<SchemaInformation>& schemas)
    {
      std::ostringstream out;
      out << "{\"schemas\":[";
      for (std::size_t schemaIndex = 0; schemaIndex < schemas.size(); ++schemaIndex) {
        if (schemaIndex != 0) {
          out << ',';
        }
        const auto& schema = schemas[schemaIndex];
        out << "{\"name\":";
        writeJsonString(out, schema.name);
        out << ",\"tables\":[";

        for (std::size_t tableIndex = 0; tableIndex < schema.tables.size(); ++tableIndex) {
          if (tableIndex != 0) {
            out << ',';
          }
          const core::SchemaTableSnapshot& table = *schema.tables[tableIndex];
          out << "{\"name\":";
          writeJsonString(out, table.name);
          out << ",\"columns\":[";
          for (std::size_t columnIndex = 0; columnIndex < table.columns.size(); ++columnIndex) {
            if (columnIndex != 0) {
              out << ',';
            }
            const auto& column = table.columns[columnIndex];
            out << "{\"name\":";
            writeJsonString(out, column.name);
            out << ",\"type\":";
            writeJsonString(out, columnTypeName(column.type));
            out << ",\"nullable\":" << std::boolalpha << column.nullable << ",\"generated\":" << column.generated
                << ",\"unique\":" << column.unique << '}';
          }
          out << "],\"primaryKey\":";
          writeJsonStringArray(out, table.primaryKey);
          out << ",\"foreignKeys\":";
          writeJsonStringArray(out, table.foreignKey);
          out << ",\"indexes\":";
          writeJsonStringArray(out, table.indexes);
          out << '}';
        }
        out << "]}";
      }
      out << "]}";
      return std::move(out).str();
    }

    [[nodiscard]]
    ExecutionReport translateSchema(
      const Invocation& invocation,
      const core::SchemaSnapshot& databaseSchema,
      const std::shared_ptr<InspectMetrics>& metrics)
    {
      const auto schemas = extractSchemas(databaseSchema, metrics);
      const bool json = invocation.global.format.value_or("text") == "json";

      return {
        .info = "Database schema inspected.",
        .status = ExecutionStatus::Success,
        .metrics = metrics,
        .renderedOutput = json ? translateJson(schemas) : translateText(schemas),
      };
    }
  } // namespace

  void InspectMetrics::writeText(std::ostream& out) const
  {
    out << "Inspect summary\n\n";
    printMetric(out, "Schemas discovered", schemasDiscovered);
    printMetric(out, "Tables discovered", tablesDiscovered);
    printMetric(out, "Columns discovered", columnsDiscovered);
    printMetric(out, "Primary key columns", primaryKeysDiscovered);
    printMetric(out, "Foreign keys", foreignKeysDiscovered);
    printMetric(out, "Indexes", indexesDiscovered);
    out << "\nTiming:\n";
    printDuration(out, "Introspection", introspectionDuration);
    printDuration(out, "Translation", discoveryDuration);
    printDuration(out, "Total", totalDuration);
  }

  void InspectMetrics::writeJson(std::ostream& out) const
  {
    out << "{\"schemasDiscovered\":" << schemasDiscovered << ",\"tablesDiscovered\":" << tablesDiscovered
        << ",\"columnsDiscovered\":" << columnsDiscovered << ",\"primaryKeysDiscovered\":" << primaryKeysDiscovered
        << ",\"foreignKeysDiscovered\":" << foreignKeysDiscovered << ",\"indexesDiscovered\":" << indexesDiscovered
        << '}';
  }

  ExecutionReport inspect(const Invocation& invocation, const core::SchemaSnapshot& databaseSchema)
  {
    const auto started = Clock::now();
    auto metrics = std::make_shared<InspectMetrics>();
    ExecutionReport report = translateSchema(invocation, databaseSchema, metrics);
    metrics->discoveryDuration = Clock::now() - started - metrics->introspectionDuration;
    metrics->totalDuration = Clock::now() - started;
    return report;
  }

  ExecutionReport inspect(const Invocation& invocation)
  {
    const auto started = Clock::now();
    const auto inspector = schemaInspector(invocation);
    const auto databaseSchema = inspector.inspect();
    auto metrics = std::make_shared<InspectMetrics>();
    metrics->introspectionDuration = Clock::now() - started;
    ExecutionReport report = translateSchema(invocation, databaseSchema, metrics);
    metrics->discoveryDuration = Clock::now() - started - metrics->introspectionDuration;
    metrics->totalDuration = Clock::now() - started;
    return report;
  }
} // namespace worm::cli::database
