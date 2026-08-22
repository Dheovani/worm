#pragma once

#include <chrono>
#include <cstddef>
#include <ostream>

#include <core/model/schema-snapshot.hpp>

#include "../runner.hpp"
#include "manifest.hpp"

namespace worm::cli::generator
{
  struct PushMetrics : public ExecutionMetrics
  {
    std::size_t entitiesDiscovered{0};
    std::size_t entitiesSelected{0};

    std::size_t existingTables{0};
    std::size_t compatibleTables{0};
    std::size_t incompatibleTables{0};
    std::size_t missingTables{0};

    std::size_t plannedTables{0};
    std::size_t createdTables{0};
    std::size_t failedTables{0};

    std::chrono::nanoseconds discoveryDuration{0};
    std::chrono::nanoseconds comparisonDuration{0};
    std::chrono::nanoseconds planningDuration{0};
    std::chrono::nanoseconds executionDuration{0};
    std::chrono::nanoseconds totalDuration{0};

    void writeText(std::ostream& out) const override;
    void writeJson(std::ostream& out) const override;
  };

  ExecutionReport push(const Invocation& invocation, const SchemaManifest& manifest);

  [[nodiscard]]
  ExecutionReport planPush(
    const Invocation& invocation,
    const SchemaManifest& manifest,
    const core::SchemaSnapshot& databaseSchema);

  ExecutionReport push(const Invocation& invocation);
} // namespace worm::cli::generator
