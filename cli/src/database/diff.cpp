#include "diff.hpp"

#include <core/model/schema-diff.hpp>
#include <errors/invalid-cli-argument-exception.hpp>
#include <helpers/connection.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace worm::cli::database
{
  namespace
  {
    using Clock = std::chrono::steady_clock;

    [[nodiscard]]
    std::string_view differenceName(core::SchemaDifferenceKind kind) noexcept
    {
      switch (kind) {
      case core::SchemaDifferenceKind::MissingTable:
        return "missing-table";
      case core::SchemaDifferenceKind::UnexpectedTable:
        return "unexpected-table";
      case core::SchemaDifferenceKind::MissingColumn:
        return "missing-column";
      case core::SchemaDifferenceKind::UnexpectedColumn:
        return "unexpected-column";
      case core::SchemaDifferenceKind::ColumnTypeMismatch:
        return "column-type-mismatch";
      case core::SchemaDifferenceKind::NullableMismatch:
        return "nullability-mismatch";
      case core::SchemaDifferenceKind::GeneratedMismatch:
        return "generated-mismatch";
      case core::SchemaDifferenceKind::UniqueMismatch:
        return "uniqueness-mismatch";
      case core::SchemaDifferenceKind::DefaultExpressionMismatch:
        return "default-mismatch";
      case core::SchemaDifferenceKind::MissingPrimaryKey:
        return "missing-primary-key";
      case core::SchemaDifferenceKind::PrimaryKeyMismatch:
        return "primary-key-mismatch";
      }
      return "unknown";
    }

    [[nodiscard]]
    std::string targetName(const core::SchemaDifference& difference)
    {
      std::string target = difference.schema.empty() ? difference.table : difference.schema + "." + difference.table;
      if (!difference.column.empty()) {
        target += "." + difference.column;
      }
      return target;
    }

    void countDifference(DiffMetrics& metrics, core::SchemaDifferenceKind kind)
    {
      ++metrics.differencesDetected;
      switch (kind) {
      case core::SchemaDifferenceKind::MissingTable:
        ++metrics.missingTables;
        break;
      case core::SchemaDifferenceKind::UnexpectedTable:
        ++metrics.unexpectedTables;
        break;
      case core::SchemaDifferenceKind::MissingColumn:
        ++metrics.missingColumns;
        break;
      case core::SchemaDifferenceKind::UnexpectedColumn:
        ++metrics.unexpectedColumns;
        break;
      case core::SchemaDifferenceKind::MissingPrimaryKey:
      case core::SchemaDifferenceKind::PrimaryKeyMismatch:
        ++metrics.primaryKeyDifferences;
        break;
      case core::SchemaDifferenceKind::ColumnTypeMismatch:
      case core::SchemaDifferenceKind::NullableMismatch:
      case core::SchemaDifferenceKind::GeneratedMismatch:
      case core::SchemaDifferenceKind::UniqueMismatch:
      case core::SchemaDifferenceKind::DefaultExpressionMismatch:
        ++metrics.metadataMismatches;
        break;
      }
    }

    [[nodiscard]]
    ExecutionReport compare(
      const SchemaManifest& manifest,
      const core::SchemaSnapshot& databaseSchema,
      const std::shared_ptr<DiffMetrics>& metrics)
    {
      const auto discoveryStarted = Clock::now();
      const core::SchemaMetadata expected = schemaMetadata(manifest);
      metrics->entitiesDiscovered = manifest.entities.size();
      metrics->tablesDiscovered = databaseSchema.tables.size();
      metrics->entitiesCompared = expected.tables().size();
      metrics->discoveryDuration += Clock::now() - discoveryStarted;

      const auto comparisonStarted = Clock::now();
      metrics->differences = core::compareSchemas(expected, databaseSchema);
      for (const core::SchemaDifference& difference : metrics->differences) {
        countDifference(*metrics, difference.kind);
      }
      metrics->comparisonDuration = Clock::now() - comparisonStarted;

      const bool drift = !metrics->differences.empty();
      return {
        .info = drift ? "Schema differences detected." : "No schema differences detected.",
        .status = drift ? ExecutionStatus::DriftDetected : ExecutionStatus::Success,
        .metrics = metrics,
      };
    }
  } // namespace

  void DiffMetrics::writeText(std::ostream& out) const
  {
    if (!differences.empty()) {
      out << "Differences:\n";
      for (const core::SchemaDifference& difference : differences) {
        out << "  - [" << differenceName(difference.kind) << "] " << targetName(difference);
        if (!difference.expected.empty() || !difference.actual.empty()) {
          out << " (expected: '" << difference.expected << "', actual: '" << difference.actual << "')";
        }
        out << '\n';
      }
      out << '\n';
    }

    out << "Diff summary\n\nDiscovery:\n";
    printMetric(out, "Entities discovered", entitiesDiscovered);
    printMetric(out, "Tables discovered", tablesDiscovered);
    printMetric(out, "Entities compared", entitiesCompared);
    out << "\nDifferences:\n";
    printMetric(out, "Total", differencesDetected);
    printMetric(out, "Missing tables", missingTables);
    printMetric(out, "Unexpected tables", unexpectedTables);
    printMetric(out, "Missing columns", missingColumns);
    printMetric(out, "Unexpected columns", unexpectedColumns);
    printMetric(out, "Metadata mismatches", metadataMismatches);
    printMetric(out, "Primary key differences", primaryKeyDifferences);
    out << "\nTiming:\n";
    printDuration(out, "Discovery", discoveryDuration);
    printDuration(out, "Introspection", introspectionDuration);
    printDuration(out, "Comparison", comparisonDuration);
    printDuration(out, "Total", totalDuration);
  }

  void DiffMetrics::writeJson(std::ostream& out) const
  {
    out << "{\"entitiesDiscovered\":" << entitiesDiscovered << ",\"tablesDiscovered\":" << tablesDiscovered
        << ",\"entitiesCompared\":" << entitiesCompared << ",\"differencesDetected\":" << differencesDetected
        << ",\"missingTables\":" << missingTables << ",\"unexpectedTables\":" << unexpectedTables
        << ",\"missingColumns\":" << missingColumns << ",\"unexpectedColumns\":" << unexpectedColumns
        << ",\"metadataMismatches\":" << metadataMismatches << ",\"primaryKeyDifferences\":" << primaryKeyDifferences
        << ",\"differences\":[";

    for (std::size_t index = 0; index < differences.size(); ++index) {
      if (index != 0) {
        out << ',';
      }
      const core::SchemaDifference& difference = differences[index];
      out << "{\"kind\":";
      writeJsonString(out, differenceName(difference.kind));
      out << ",\"target\":";
      writeJsonString(out, targetName(difference));
      out << ",\"expected\":";
      writeJsonString(out, difference.expected);
      out << ",\"actual\":";
      writeJsonString(out, difference.actual);
      out << '}';
    }
    out << "]}";
  }

  ExecutionReport
  diff(const Invocation& invocation, const SchemaManifest& manifest, const core::SchemaSnapshot& databaseSchema)
  {
    static_cast<void>(invocation);
    const auto started = Clock::now();
    auto metrics = std::make_shared<DiffMetrics>();
    ExecutionReport report = compare(manifest, databaseSchema, metrics);
    metrics->totalDuration = Clock::now() - started;
    return report;
  }

  ExecutionReport diff(const Invocation& invocation)
  {
    const auto started = Clock::now();
    if (!invocation.global.manifest.has_value()) {
      throw InvalidCliArgumentException("The 'diff' command requires a schema manifest.");
    }

    const connection::DatabaseType type = databaseType(invocation);
    const std::string schema =
      type == connection::DatabaseType::MySQL ? invocation.global.database.value_or("") : defaultSchema(type);

    const auto discoveryStarted = Clock::now();
    const SchemaManifest manifest = loadManifest(*invocation.global.manifest, schema);
    const auto discoveryDuration = Clock::now() - discoveryStarted;

    const auto introspectionStarted = Clock::now();
    const core::SchemaSnapshot databaseSchema = schemaInspector(invocation).inspect();
    const auto introspectionDuration = Clock::now() - introspectionStarted;

    auto metrics = std::make_shared<DiffMetrics>();
    metrics->discoveryDuration = discoveryDuration;
    metrics->introspectionDuration = introspectionDuration;
    ExecutionReport report = compare(manifest, databaseSchema, metrics);
    metrics->totalDuration = Clock::now() - started;
    return report;
  }
} // namespace worm::cli::database
