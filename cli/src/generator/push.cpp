#include "push.hpp"

#include <memory>

namespace worm::cli::generator
{
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
    out << "{}";
  }

  ExecutionReport push(const Invocation&)
  {
    return {
      .info = "The 'push' command is not implemented yet.",
      .status = ExecutionStatus::Blocked,
      .metrics = std::make_shared<PushMetrics>(),
    };
  }
} // namespace worm::cli::generator
