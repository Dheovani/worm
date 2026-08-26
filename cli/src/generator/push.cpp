#include "push.hpp"

#include <connection/schema-inspector.hpp>
#include <core/model/schema-metadata.hpp>
#include <core/model/schema-snapshot.hpp>
#include <core/persistence/repository.hpp>
#include <core/query/query-builder.hpp>
#include <errors/worm-exception.hpp>
#include <utils/dependency-injection.hpp>

#include <algorithm>
#include <chrono>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "../errors/invalid-cli-argument-exception.hpp"
#include <helpers/connection.hpp>

namespace worm::cli::generator
{
  namespace
  {
    using Clock = std::chrono::steady_clock;

    struct CreationFailure
    {
      std::string table;
      std::string reason;
    };

    [[nodiscard]]
    std::string tableLabel(const core::Table& table)
    {
      if (table.schema().empty()) {
        return std::string{table.name()};
      }
      return std::string{table.schema().name()} + "." + std::string{table.name()};
    }

    [[nodiscard]]
    std::string reportInfo(
      const std::vector<CreationFailure>& failures,
      std::size_t plannedTables,
      std::size_t incompatibleTables,
      bool apply)
    {
      std::ostringstream out;
      if (!failures.empty()) {
        out << "Schema push failed for " << failures.size() << " table(s):";

        for (const CreationFailure& failure : failures) {
          out << "\n  - " << failure.table << ": " << failure.reason;
        }

        return out.str();
      }

      if (incompatibleTables != 0) {
        return "Existing tables differ from the manifest. No destructive or ambiguous change was applied.";
      }

      if (plannedTables == 0) {
        return "The database schema is already compatible with the selected entities.";
      }

      if (!apply) {
        out << "Schema push plan contains " << plannedTables << " table(s). Re-run with --apply to create them.";
        return out.str();
      }

      out << "Created " << plannedTables << " table(s).";
      return out.str();
    }

    void validateSelections(const Invocation& invocation, const SchemaManifest& manifest)
    {
      for (const std::string& requestedEntity : invocation.arguments.entities) {
        const bool found = std::ranges::any_of(
          manifest.entities, [&](const ManifestEntity& entity) { return entity.name == requestedEntity; });
        if (!found) {
          throw InvalidCliArgumentException("Unknown manifest entity '{}'.", requestedEntity);
        }
      }
    }

    [[nodiscard]]
    std::vector<const core::TableMetadata*> creationOrder(std::vector<const core::TableMetadata*> tables)
    {
      std::vector<const core::TableMetadata*> ordered;
      ordered.reserve(tables.size());

      while (!tables.empty()) {
        const auto ready = std::find_if(tables.begin(), tables.end(), [&](const core::TableMetadata* candidate) {
          if (candidate == nullptr) {
            return true;
          }

          return std::ranges::none_of(candidate->foreignKeys(), [&](const core::ForeignKey& foreignKey) {
            if (foreignKey.referencedTable() == candidate->table()) {
              return false;
            }

            return std::ranges::any_of(tables, [&](const core::TableMetadata* pending) {
              return pending != nullptr && pending->table() == foreignKey.referencedTable();
            });
          });
        });

        if (ready == tables.end()) {
          throw InvalidCliArgumentException(
            "The selected schema contains a foreign-key cycle that cannot be created inline.");
        }

        ordered.push_back(*ready);
        tables.erase(ready);
      }

      return ordered;
    }

    [[nodiscard]]
    bool tablesCompatible(const core::SchemaTableSnapshot& expected, const core::SchemaTableSnapshot& actual)
    {
      if (expected.primaryKey != actual.primaryKey || expected.columns.size() != actual.columns.size()) {
        return false;
      }

      for (const core::SchemaColumnSnapshot& expectedColumn : expected.columns) {
        const core::SchemaColumnSnapshot* actualColumn = actual.findColumn(expectedColumn.name);
        if (actualColumn == nullptr) {
          return false;
        }

        const bool typeMatches =
          expectedColumn.type.kind == core::ColumnTypeKind::Unknown ||
          expectedColumn.type.kind == actualColumn->type.kind;
        const bool constraintsMatch =
          expectedColumn.nullable == actualColumn->nullable &&
          expectedColumn.generated == actualColumn->generated &&
          expectedColumn.unique == actualColumn->unique;

        if (!typeMatches || !constraintsMatch) {
          return false;
        }
      }

      return true;
    }

    [[nodiscard]]
    ExecutionReport updateDatabase(
      const Invocation& invocation,
      const SchemaManifest& manifest,
      const core::SchemaSnapshot& databaseSchema,
      core::Repository<core::SchemaMetadata>* repository,
      const std::shared_ptr<PushMetrics>& metrics)
    {
      validateSelections(invocation, manifest);
      metrics->entitiesDiscovered = manifest.entities.size();

      const auto planningStarted = Clock::now();
      const core::SchemaMetadata desiredSchema = schemaMetadata(manifest);
      std::vector<const core::TableMetadata*> missingTables;
      metrics->planningDuration = Clock::now() - planningStarted;

      const auto comparisonStarted = Clock::now();
      for (const ManifestEntity& entity : manifest.entities) {
        if (!invocation.arguments.entities.empty() &&
            std::ranges::find(invocation.arguments.entities, entity.name) == invocation.arguments.entities.end()) {
          continue;
        }

        ++metrics->entitiesSelected;
        const core::Table table{core::Schema{entity.table.schema}, entity.table.name};
        const core::TableMetadata* tableMetadata = desiredSchema.findTable(table);
        const core::SchemaTableSnapshot* tableSnapshot =
          databaseSchema.findTable(entity.table.schema, entity.table.name);

        if (tableSnapshot == nullptr) {
          ++metrics->missingTables;
          ++metrics->plannedTables;
          missingTables.push_back(tableMetadata);
          continue;
        }

        ++metrics->existingTables;
        if (tablesCompatible(entity.table, *tableSnapshot)) {
          ++metrics->compatibleTables;
        } else {
          ++metrics->incompatibleTables;
        }
      }
      metrics->comparisonDuration = Clock::now() - comparisonStarted;

      missingTables = creationOrder(std::move(missingTables));

      std::vector<CreationFailure> failures;
      if (invocation.arguments.apply && repository != nullptr) {
        const auto executionStarted = Clock::now();
        for (const core::TableMetadata* table : missingTables) {
          if (table == nullptr) {
            failures.push_back({"<unknown>", "Manifest table metadata could not be resolved."});
            ++metrics->failedTables;
            continue;
          }

          try {
            repository->create(*table);
            ++metrics->createdTables;
          } catch (const worm::WormException& error) {
            failures.push_back({tableLabel(table->table()), error.what()});
            ++metrics->failedTables;
          }
        }
        metrics->executionDuration = Clock::now() - executionStarted;
      }

      ExecutionStatus status = ExecutionStatus::Success;
      if (!failures.empty()) {
        status = ExecutionStatus::Failed;
      } else if (metrics->incompatibleTables != 0) {
        status = ExecutionStatus::DriftDetected;
      }

      return {
        .info = reportInfo(failures, metrics->plannedTables, metrics->incompatibleTables, invocation.arguments.apply),
        .status = status,
        .metrics = metrics,
      };
    }
  } // namespace

  void PushMetrics::writeText(std::ostream& out) const
  {
    out << "Push summary\n\n";
    out << "Discovery:\n";
    printMetric(out, "Entities discovered", entitiesDiscovered);
    printMetric(out, "Entities selected", entitiesSelected);
    out << "\nDatabase state:\n";
    printMetric(out, "Existing tables", existingTables);
    printMetric(out, "Compatible tables", compatibleTables);
    printMetric(out, "Incompatible tables", incompatibleTables);
    printMetric(out, "Missing tables", missingTables);
    out << "\nChanges:\n";
    printMetric(out, "Planned tables", plannedTables);
    printMetric(out, "Created tables", createdTables);
    printMetric(out, "Failed tables", failedTables);
    out << "\nTiming:\n";
    printDuration(out, "Discovery", discoveryDuration);
    printDuration(out, "Comparison", comparisonDuration);
    printDuration(out, "Planning", planningDuration);
    printDuration(out, "Execution", executionDuration);
    printDuration(out, "Total", totalDuration);
  }

  void PushMetrics::writeJson(std::ostream& out) const
  {
    out << "{\"entitiesDiscovered\":" << entitiesDiscovered << ",\"entitiesSelected\":" << entitiesSelected
        << ",\"existingTables\":" << existingTables << ",\"compatibleTables\":" << compatibleTables
        << ",\"incompatibleTables\":" << incompatibleTables << ",\"missingTables\":" << missingTables
        << ",\"plannedTables\":" << plannedTables << ",\"createdTables\":" << createdTables
        << ",\"failedTables\":" << failedTables << "}";
  }

  ExecutionReport push(const Invocation& invocation, const SchemaManifest& manifest)
  {
    const auto started = Clock::now();
    const connection::DatabaseType type = databaseType(invocation);
    std::shared_ptr<connection::Client> client =
      DependencyInjector<connection::Client>::get(connectionConfig(invocation, type), type);
    const core::SchemaSnapshot databaseSchema = connection::SchemaInspector{*client}.inspect();
    auto metrics = std::make_shared<PushMetrics>();

    if (!invocation.arguments.apply) {
      ExecutionReport report = updateDatabase(invocation, manifest, databaseSchema, nullptr, metrics);
      metrics->discoveryDuration = Clock::now() - started - metrics->planningDuration - metrics->comparisonDuration;
      metrics->totalDuration = Clock::now() - started;
      return report;
    }

    const core::QueryBuilder queryBuilder = DependencyInjector<core::QueryBuilder>::get(type);
    core::Repository<core::SchemaMetadata> repository{client, queryBuilder};
    ExecutionReport report = updateDatabase(invocation, manifest, databaseSchema, &repository, metrics);
    metrics->discoveryDuration =
      Clock::now() - started - metrics->planningDuration - metrics->comparisonDuration - metrics->executionDuration;
    metrics->totalDuration = Clock::now() - started;
    return report;
  }

  ExecutionReport planPush(
    const Invocation& invocation,
    const SchemaManifest& manifest,
    const core::SchemaSnapshot& databaseSchema)
  {
    if (invocation.arguments.apply) {
      throw InvalidCliArgumentException("A push plan cannot apply schema changes.");
    }

    auto metrics = std::make_shared<PushMetrics>();
    ExecutionReport report = updateDatabase(invocation, manifest, databaseSchema, nullptr, metrics);
    metrics->totalDuration = metrics->planningDuration + metrics->comparisonDuration;
    return report;
  }

  ExecutionReport push(const Invocation& invocation)
  {
    if (!invocation.global.manifest.has_value()) {
      throw InvalidCliArgumentException("The 'push' command requires a schema manifest.");
    }

    const connection::DatabaseType type = databaseType(invocation);
    const std::string schema =
      type == connection::DatabaseType::MySQL ? invocation.global.database.value_or("") : defaultSchema(type);
    const SchemaManifest manifest = loadManifest(*invocation.global.manifest, schema);
    return push(invocation, manifest);
  }
} // namespace worm::cli::generator
