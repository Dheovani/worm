#pragma once

#include <chrono>
#include <cstddef>
#include <ostream>

#include "../runner.hpp"

namespace worm::cli::generator
{
  struct PullMetrics : public ExecutionMetrics
  {
    std::size_t tablesDiscovered{0};
    std::size_t tablesSelected{0};

    std::size_t existingEntities{0};
    std::size_t compatibleEntities{0};
    std::size_t incompatibleEntities{0};
    std::size_t missingEntities{0};

    std::size_t plannedEntities{0};
    std::size_t generatedEntities{0};
    std::size_t failedEntities{0};

    std::chrono::nanoseconds discoveryDuration{0};
    std::chrono::nanoseconds comparisonDuration{0};
    std::chrono::nanoseconds planningDuration{0};
    std::chrono::nanoseconds executionDuration{0};
    std::chrono::nanoseconds totalDuration{0};

    void writeText(std::ostream& out) const override;
    void writeJson(std::ostream& out) const override;
  };

  ExecutionReport pull(const Invocation& invocation);
} // namespace worm::cli::generator
