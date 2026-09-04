#pragma once

#include <core/model/schema-diff.hpp>

#include <chrono>
#include <cstddef>
#include <ostream>
#include <vector>

#include <helpers/manifest.hpp>
#include <runner.hpp>

namespace worm::cli::database
{
  struct DiffMetrics final : public ExecutionMetrics
  {
    std::size_t entitiesDiscovered{0};
    std::size_t tablesDiscovered{0};
    std::size_t entitiesCompared{0};

    std::size_t differencesDetected{0};
    std::size_t missingTables{0};
    std::size_t unexpectedTables{0};
    std::size_t missingColumns{0};
    std::size_t unexpectedColumns{0};
    std::size_t metadataMismatches{0};
    std::size_t primaryKeyDifferences{0};

    std::chrono::nanoseconds discoveryDuration{0};
    std::chrono::nanoseconds introspectionDuration{0};
    std::chrono::nanoseconds comparisonDuration{0};
    std::chrono::nanoseconds totalDuration{0};
    std::vector<core::SchemaDifference> differences;

    void writeText(std::ostream& out) const override;
    void writeJson(std::ostream& out) const override;
  };

  [[nodiscard]]
  ExecutionReport
  diff(const Invocation& invocation, const SchemaManifest& manifest, const core::SchemaSnapshot& databaseSchema);

  [[nodiscard]]
  ExecutionReport diff(const Invocation& invocation);
} // namespace worm::cli::database
