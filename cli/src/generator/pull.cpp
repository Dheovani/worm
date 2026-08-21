#include "pull.hpp"

#include <memory>

namespace worm::cli::generator
{
  void PullMetrics::writeText(std::ostream& out) const
  {
    out << "Pull summary\n\n";

    out << "Discovery:\n";
    printMetric(out, "Tables discovered", tablesDiscovered);
    printMetric(out, "Tables selected", tablesSelected);

    out << "\nCode state:\n";
    printMetric(out, "Existing entities", existingEntities);
    printMetric(out, "Compatible entities", compatibleEntities);
    printMetric(out, "Incompatible entities", incompatibleEntities);
    printMetric(out, "Missing entities", missingEntities);

    out << "\nChanges:\n";
    printMetric(out, "Planned entities", plannedEntities);
    printMetric(out, "Generated entities", generatedEntities);
    printMetric(out, "Failed entities", failedEntities);

    out << "\nTiming:\n";
    printDuration(out, "Discovery", discoveryDuration);
    printDuration(out, "Comparison", comparisonDuration);
    printDuration(out, "Planning", planningDuration);
    printDuration(out, "Execution", executionDuration);
    printDuration(out, "Total", totalDuration);
  }

  void PullMetrics::writeJson(std::ostream& out) const
  {
    out << "{}";
  }

  ExecutionReport pull(const Invocation&)
  {
    return {
      .info = "The 'pull' command is not implemented yet.",
      .status = ExecutionStatus::Blocked,
      .metrics = std::make_shared<PullMetrics>(),
    };
  }
} // namespace worm::cli::generator
