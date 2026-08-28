#pragma once

#include <core/model/schema-snapshot.hpp>
#include <runner.hpp>

namespace worm::cli::database
{
  struct InspectMetrics final : public ExecutionMetrics
  {
    std::size_t schemasDiscovered{0};
    std::size_t tablesDiscovered{0};
    std::size_t columnsDiscovered{0};

    std::size_t primaryKeysDiscovered{0};
    std::size_t foreignKeysDiscovered{0};
    std::size_t indexesDiscovered{0};

    std::chrono::nanoseconds introspectionDuration{0};
    std::chrono::nanoseconds discoveryDuration{0};
    std::chrono::nanoseconds totalDuration{0};

    void writeText(std::ostream& out) const override;
    void writeJson(std::ostream& out) const override;
  };

  [[nodiscard]]
  ExecutionReport inspect(const Invocation& invocation, const core::SchemaSnapshot& databaseSchema);

  [[nodiscard]]
  ExecutionReport inspect(const Invocation& invocation);
} // namespace worm::cli::database
