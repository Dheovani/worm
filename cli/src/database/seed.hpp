#pragma once

#include <core/model/schema-snapshot.hpp>

#include <chrono>
#include <cstddef>
#include <ostream>

#include <runner.hpp>

namespace worm::cli::database
{
  struct SeedMetrics final : public ExecutionMetrics
  {
    std::size_t tablesDiscovered{0};
    std::size_t tablesSelected{0};
    std::size_t dependencyTables{0};

    std::size_t requestedRows{0};
    std::size_t plannedRows{0};
    std::size_t dependencyRows{0};

    std::size_t generatedValues{0};
    std::size_t providedValues{0};
    std::size_t databaseGeneratedValues{0};

    std::size_t generatedStatements{0};
    std::size_t executedStatements{0};
    std::size_t insertedRows{0};

    std::chrono::nanoseconds introspectionDuration{};
    std::chrono::nanoseconds planningDuration{};
    std::chrono::nanoseconds generationDuration{};
    std::chrono::nanoseconds executionDuration{};
    std::chrono::nanoseconds totalDuration{};

    void writeText(std::ostream& out) const override;
    void writeJson(std::ostream& out) const override;
  };

  [[nodiscard]]
  ExecutionReport seed(const Invocation& invocation);

  [[nodiscard]]
  ExecutionReport seed(const Invocation& invocation, const core::SchemaSnapshot& databaseSchema);
} // namespace worm::cli::database
