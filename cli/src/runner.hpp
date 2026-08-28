#pragma once

#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

#include "parser.hpp"

namespace worm::cli
{
  enum class ExecutionStatus
  {
    Success,
    IssuesDetected,
    DriftDetected,
    Blocked,
    Failed
  };

  class ExecutionMetrics
  {
  public:
    virtual ~ExecutionMetrics() = default;

    virtual void writeText(std::ostream& out) const = 0;
    virtual void writeJson(std::ostream& out) const = 0;

    [[nodiscard]]
    static double milliseconds(std::chrono::nanoseconds duration) noexcept;

    static void printDuration(std::ostream& out, std::string_view label, std::chrono::nanoseconds duration);
    static void printMetric(std::ostream& out, std::string_view label, std::size_t value);
    static void writeJsonString(std::ostream& out, std::string_view value);
  };

  struct ExecutionReport
  {
    std::string command;
    std::string info;
    ExecutionStatus status{ExecutionStatus::Failed};
    std::shared_ptr<const ExecutionMetrics> metrics;
    std::optional<std::string> renderedOutput;
  };

  void printUsage() noexcept;

  void printSystemVersion() noexcept;

  [[nodiscard]]
  std::string buildCommand(int argc, char* const argv[]);

  [[nodiscard]]
  int exitCode(ExecutionStatus status) noexcept;

  void outputReport(const ExecutionReport& report, std::string_view format, std::ostream& out);

  [[nodiscard]]
  int execute(const Invocation& invocation, int argc, char* const argv[], std::ostream& out);
} // namespace worm::cli
