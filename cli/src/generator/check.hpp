#pragma once

#include <core/model/schema-snapshot.hpp>

#include <chrono>
#include <cstddef>
#include <ostream>
#include <string>
#include <vector>

#include "../parser.hpp"
#include "../runner.hpp"
#include "manifest.hpp"

namespace worm::cli::generator
{
  struct CheckMetrics final : public ExecutionMetrics
  {
    std::size_t entitiesDiscovered{0};
    std::size_t tablesDiscovered{0};
    std::size_t entitiesSelected{0};
    std::size_t tablesSelected{0};
    std::size_t matchedObjects{0};
    std::size_t compatibleObjects{0};
    std::size_t incompatibleObjects{0};
    std::size_t missingInCode{0};
    std::size_t missingInDatabase{0};
    std::chrono::nanoseconds discoveryDuration{0};
    std::chrono::nanoseconds comparisonDuration{0};
    std::chrono::nanoseconds totalDuration{0};
    std::vector<std::string> differences;

    void writeText(std::ostream& out) const override;
    void writeJson(std::ostream& out) const override;
  };

  [[nodiscard]]
  ExecutionReport check(
    const Invocation& invocation,
    const SchemaManifest& manifest,
    const core::SchemaSnapshot& databaseSchema);

  [[nodiscard]]
  ExecutionReport check(const Invocation& invocation);
} // namespace worm::cli::generator
