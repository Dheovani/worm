#include "runner.hpp"

#include <iomanip>
#include <sstream>

#include "errors/invalid-argument-exception.hpp"
#include "generator/check.hpp"
#include "generator/pull.hpp"

namespace worm::cli
{
  namespace
  {
    [[nodiscard]]
    std::string_view statusName(ExecutionStatus status) noexcept
    {
      switch (status) {
      case ExecutionStatus::Success:
        return "success";
      case ExecutionStatus::DriftDetected:
        return "drift";
      case ExecutionStatus::Blocked:
        return "blocked";
      case ExecutionStatus::Failed:
        return "failed";
      }

      return "failed";
    }
  } // namespace

  double ExecutionMetrics::milliseconds(std::chrono::nanoseconds duration) noexcept
  {
    return std::chrono::duration<double, std::milli>{duration}.count();
  }

  void ExecutionMetrics::printDuration(std::ostream& out, std::string_view label, std::chrono::nanoseconds duration)
  {
    out << "  " << std::left << std::setw(24) << label << std::right << std::fixed << std::setprecision(2)
        << milliseconds(duration) << " ms\n";
  }

  void ExecutionMetrics::printMetric(std::ostream& out, std::string_view label, std::size_t value)
  {
    out << "  " << std::left << std::setw(24) << label << std::right << value << '\n';
  }

  void ExecutionMetrics::writeJsonString(std::ostream& out, std::string_view value)
  {
    out << '"';
    for (const char character : value) {
      switch (character) {
      case '"':
        out << "\\\"";
        break;
      case '\\':
        out << "\\\\";
        break;
      case '\n':
        out << "\\n";
        break;
      case '\r':
        out << "\\r";
        break;
      case '\t':
        out << "\\t";
        break;
      default:
        out << character;
      }
    }
    out << '"';
  }

  std::string buildCommand(int argc, char* const argv[])
  {
    std::ostringstream command;
    for (int index = 0; index < argc; ++index) {
      if (index != 0) {
        command << ' ';
      }

      const std::string_view argument = argv[index];
      if (argument.starts_with("--password=")) {
        command << "--password=<redacted>";
      } else {
        command << argument;
      }

      if (argument == "--password" && index + 1 < argc) {
        command << " <redacted>";
        ++index;
      }
    }
    return command.str();
  }

  int exitCode(ExecutionStatus status) noexcept
  {
    switch (status) {
    case ExecutionStatus::Success:
      return 0;
    case ExecutionStatus::Failed:
      return 1;
    case ExecutionStatus::DriftDetected:
      return 2;
    case ExecutionStatus::Blocked:
      return 3;
    }

    return 1;
  }

  void outputReport(const ExecutionReport& report, std::string_view format, std::ostream& out)
  {
    if (report.metrics == nullptr) {
      throw InvalidArgumentException("Execution report has no metrics.");
    }

    if (format == "json") {
      out << "{\"command\":";
      ExecutionMetrics::writeJsonString(out, report.command);
      out << ",\"status\":";
      ExecutionMetrics::writeJsonString(out, statusName(report.status));
      out << ",\"info\":";
      ExecutionMetrics::writeJsonString(out, report.info);
      out << ",\"metrics\":";
      report.metrics->writeJson(out);
      out << "}\n";
      return;
    }

    out << "Worm\n\n" << report.info << "\n\n";
    report.metrics->writeText(out);
  }

  int execute(const Invocation& invocation, int argc, char* const argv[], std::ostream& out)
  {
    ExecutionReport report;
    switch (invocation.command) {
    case Commands::Check:
      report = generator::check(invocation);
      break;
    case Commands::Pull:
      report = generator::pull(invocation);
      break;
    case Commands::Push:
      throw InvalidArgumentException("The 'push' command is not implemented at this stage.");
    }
    report.command = buildCommand(argc, argv);
    outputReport(report, invocation.global.format.value_or("text"), out);
    return exitCode(report.status);
  }
} // namespace worm::cli
