#include "check.hpp"

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <string_view>

#include "../errors/invalid-cli-argument-exception.hpp"
#include <helpers/connection.hpp>

namespace worm::cli::generator
{
  namespace
  {
    using Clock = std::chrono::steady_clock;

    [[nodiscard]]
    bool contains(const std::vector<std::string>& values, std::string_view value)
    {
      return std::find(values.begin(), values.end(), value) != values.end();
    }

    [[nodiscard]]
    std::string tableLabel(const core::SchemaTableSnapshot& table)
    {
      return table.schema.empty() ? table.name : table.schema + "." + table.name;
    }

    [[nodiscard]]
    bool selectedEntity(const Invocation& invocation, const ManifestEntity& entity)
    {
      if (!invocation.arguments.entities.empty()) {
        return contains(invocation.arguments.entities, entity.name);
      }

      if (!invocation.arguments.tables.empty()) {
        return contains(invocation.arguments.tables, entity.table.name);
      }

      return true;
    }

    [[nodiscard]]
    bool selectedTable(const Invocation& invocation,
      const core::SchemaTableSnapshot& table,
      const std::vector<const ManifestEntity*>& selectedEntities)
    {
      if (!invocation.arguments.tables.empty()) {
        return contains(invocation.arguments.tables, table.name);
      }

      if (!invocation.arguments.entities.empty()) {
        return std::any_of(selectedEntities.begin(), selectedEntities.end(), [&](const auto* entity) {
          return entity->table.schema == table.schema && entity->table.name == table.name;
        });
      }

      return true;
    }

    void addDifference(CheckMetrics& metrics, std::string difference)
    {
      metrics.differences.push_back(std::move(difference));
    }

    void validateSelections(
      const Invocation& invocation, const SchemaManifest& manifest, const core::SchemaSnapshot& databaseSchema)
    {
      for (const auto& requestedEntity : invocation.arguments.entities) {
        const bool exists = std::any_of(manifest.entities.begin(), manifest.entities.end(), [&](const auto& entity) {
          return entity.name == requestedEntity;
        });
        if (!exists) {
          throw InvalidCliArgumentException("Unknown manifest entity '{}'.", requestedEntity);
        }
      }

      for (const auto& requestedTable : invocation.arguments.tables) {
        const bool existsInCode = std::any_of(manifest.entities.begin(),
          manifest.entities.end(),
          [&](const auto& entity) { return entity.table.name == requestedTable; });
        const bool existsInDatabase = std::any_of(databaseSchema.tables.begin(),
          databaseSchema.tables.end(),
          [&](const auto& table) { return table.name == requestedTable; });
        if (!existsInCode && !existsInDatabase) {
          throw InvalidCliArgumentException("Unknown table '{}'.", requestedTable);
        }
      }
    }

    [[nodiscard]]
    bool compareTables(
      const core::SchemaTableSnapshot& expected, const core::SchemaTableSnapshot& actual, CheckMetrics& metrics)
    {
      bool compatible = true;
      const std::string label = tableLabel(expected);

      for (const auto& expectedColumn : expected.columns) {
        const auto* actualColumn = actual.findColumn(expectedColumn.name);
        if (actualColumn == nullptr) {
          addDifference(metrics, label + ": column '" + expectedColumn.name + "' is missing in the database");
          compatible = false;
          continue;
        }

        if (expectedColumn.nullable != actualColumn->nullable) {
          addDifference(metrics, label + "." + expectedColumn.name + ": nullability differs");
          compatible = false;
        }
        if (expectedColumn.type.kind != core::ColumnTypeKind::Unknown &&
            expectedColumn.type.kind != actualColumn->type.kind) {
          addDifference(metrics,
            label + "." + expectedColumn.name + ": type differs (expected " +
              std::string{core::columnTypeKindName(expectedColumn.type.kind)} + ", found " +
              std::string{core::columnTypeKindName(actualColumn->type.kind)} + ")");
          compatible = false;
        }
        if (expectedColumn.generated != actualColumn->generated) {
          addDifference(metrics, label + "." + expectedColumn.name + ": generated-column state differs");
          compatible = false;
        }
        if (expectedColumn.unique != actualColumn->unique) {
          addDifference(metrics, label + "." + expectedColumn.name + ": uniqueness differs");
          compatible = false;
        }
      }

      for (const auto& actualColumn : actual.columns) {
        if (expected.findColumn(actualColumn.name) == nullptr) {
          addDifference(metrics, label + ": unexpected database column '" + actualColumn.name + "'");
          compatible = false;
        }
      }

      if (expected.primaryKey != actual.primaryKey) {
        addDifference(metrics, label + ": primary key differs");
        compatible = false;
      }

      return compatible;
    }

    [[nodiscard]]
    ExecutionReport compareSchemas(const Invocation& invocation,
      const SchemaManifest& manifest,
      const core::SchemaSnapshot& databaseSchema,
      const std::shared_ptr<CheckMetrics>& metrics)
    {
      const auto comparisonStarted = Clock::now();
      validateSelections(invocation, manifest, databaseSchema);
      metrics->entitiesDiscovered = manifest.entities.size();
      metrics->tablesDiscovered = databaseSchema.tables.size();

      std::vector<const ManifestEntity*> selectedEntities;
      for (const auto& entity : manifest.entities) {
        if (selectedEntity(invocation, entity)) {
          selectedEntities.push_back(&entity);
        }
      }
      metrics->entitiesSelected = selectedEntities.size();

      std::vector<const core::SchemaTableSnapshot*> selectedTables;
      for (const auto& table : databaseSchema.tables) {
        if (selectedTable(invocation, table, selectedEntities)) {
          selectedTables.push_back(&table);
        }
      }
      metrics->tablesSelected = selectedTables.size();

      for (const ManifestEntity* entity : selectedEntities) {
        const auto* table = databaseSchema.findTable(entity->table.schema, entity->table.name);
        if (table == nullptr ||
            std::find(selectedTables.begin(), selectedTables.end(), table) == selectedTables.end()) {
          ++metrics->missingInDatabase;
          addDifference(*metrics, entity->name + ": database table '" + tableLabel(entity->table) + "' is missing");
          continue;
        }

        ++metrics->matchedObjects;
        if (compareTables(entity->table, *table, *metrics)) {
          ++metrics->compatibleObjects;
        } else {
          ++metrics->incompatibleObjects;
        }
      }

      for (const auto* table : selectedTables) {
        const bool represented = std::any_of(selectedEntities.begin(), selectedEntities.end(), [&](const auto* entity) {
          return entity->table.schema == table->schema && entity->table.name == table->name;
        });
        if (!represented) {
          ++metrics->missingInCode;
          addDifference(*metrics, tableLabel(*table) + ": C++ entity is missing");
        }
      }

      metrics->comparisonDuration = Clock::now() - comparisonStarted;
      metrics->totalDuration = metrics->comparisonDuration;
      const bool drift =
        metrics->missingInCode != 0 || metrics->missingInDatabase != 0 || metrics->incompatibleObjects != 0;

      return {
        .info = drift ? "Schema drift detected." : "Code and database schemas are compatible.",
        .status = drift ? ExecutionStatus::DriftDetected : ExecutionStatus::Success,
        .metrics = metrics,
      };
    }
  } // namespace

  void CheckMetrics::writeText(std::ostream& out) const
  {
    if (!differences.empty()) {
      out << "Differences:\n";
      for (const auto& difference : differences) {
        out << "  - " << difference << '\n';
      }
      out << '\n';
    }

    out << "Check summary\n\nDiscovery:\n";
    printMetric(out, "Entities discovered", entitiesDiscovered);
    printMetric(out, "Tables discovered", tablesDiscovered);
    printMetric(out, "Entities selected", entitiesSelected);
    printMetric(out, "Tables selected", tablesSelected);
    out << "\nComparison:\n";
    printMetric(out, "Matched objects", matchedObjects);
    printMetric(out, "Compatible objects", compatibleObjects);
    printMetric(out, "Incompatible objects", incompatibleObjects);
    printMetric(out, "Missing in code", missingInCode);
    printMetric(out, "Missing in database", missingInDatabase);
    out << "\nTiming:\n";
    printDuration(out, "Discovery", discoveryDuration);
    printDuration(out, "Comparison", comparisonDuration);
    printDuration(out, "Total", totalDuration);
  }

  void CheckMetrics::writeJson(std::ostream& out) const
  {
    out << "{\"entitiesDiscovered\":" << entitiesDiscovered << ",\"tablesDiscovered\":" << tablesDiscovered
        << ",\"entitiesSelected\":" << entitiesSelected << ",\"tablesSelected\":" << tablesSelected
        << ",\"matched\":" << matchedObjects << ",\"compatible\":" << compatibleObjects
        << ",\"incompatible\":" << incompatibleObjects << ",\"missingInCode\":" << missingInCode
        << ",\"missingInDatabase\":" << missingInDatabase << ",\"differences\":[";

    for (std::size_t index = 0; index < differences.size(); ++index) {
      if (index != 0) {
        out << ',';
      }
      writeJsonString(out, differences[index]);
    }
    out << "]}";
  }

  ExecutionReport check(
    const Invocation& invocation,
    const SchemaManifest& manifest,
    const core::SchemaSnapshot& databaseSchema)
  {
    auto metrics = std::make_shared<CheckMetrics>();
    return compareSchemas(invocation, manifest, databaseSchema, metrics);
  }

  ExecutionReport check(const Invocation& invocation)
  {
    const auto started = Clock::now();
    if (!invocation.global.manifest.has_value()) {
      throw InvalidCliArgumentException("The 'check' command requires a schema manifest.");
    }

    const connection::DatabaseType type = databaseType(invocation);
    const std::string schema =
      type == connection::DatabaseType::MySQL ? invocation.global.database.value_or("") : defaultSchema(type);
    const SchemaManifest manifest = loadManifest(*invocation.global.manifest, schema);
    auto inspector = schemaInspector(invocation);
    const core::SchemaSnapshot databaseSchema = inspector.inspect();
    auto metrics = std::make_shared<CheckMetrics>();
    ExecutionReport report = compareSchemas(invocation, manifest, databaseSchema, metrics);
    metrics->discoveryDuration = Clock::now() - started - metrics->comparisonDuration;
    metrics->totalDuration = Clock::now() - started;
    return report;
  }
} // namespace worm::cli::generator
