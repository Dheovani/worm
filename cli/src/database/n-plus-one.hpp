#pragma once

#include <vector>

#include "../runner.hpp"

namespace worm::cli::database
{
  struct NPlusOneMetrics final : public ExecutionMetrics
  {
    std::size_t queriesDiscovered{0};
    std::size_t queriesAnalyzed{0};

    std::size_t queryPatterns{0};
    std::size_t repeatedPatterns{0};
    std::size_t potentialNPlusOnePatterns{0};
    std::size_t affectedQueries{0};

    std::chrono::nanoseconds parsingDuration{0};
    std::chrono::nanoseconds analysisDuration{0};
    std::chrono::nanoseconds totalDuration{0};

    std::vector<std::string> findings;

    void writeText(std::ostream& out) const override;
    void writeJson(std::ostream& out) const override;
  };

  ExecutionReport verify(const Invocation& invocation);
} // namespace worm::cli::database
