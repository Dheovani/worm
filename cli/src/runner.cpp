#include "runner.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>

#include "database/diff.hpp"
#include "database/inspect.hpp"
#include "database/n-plus-one.hpp"
#include "errors/invalid-cli-argument-exception.hpp"
#include "generator/check.hpp"
#include "generator/pull.hpp"
#include "generator/push.hpp"

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
      case ExecutionStatus::IssuesDetected:
        return "issues";
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

  void printUsage() noexcept
  {
    std::cout << "Worm CLI\n"
              << '\n'
              << "Usage:\n"
              << "  worm [global-options] <command> [command-options]\n"
              << '\n'
              << "Commands:\n"
              << "  check                 Compare C++ entities with the database schema\n"
              << "  push                  Generate missing database objects from C++ entities\n"
              << "  pull                  Generate missing C++ entities from database tables\n"
              << "  diff                  Display a migration-oriented schema difference report\n"
              << "  inspect               Print the complete supported database structure\n"
              << "  n-plus-one            Detect repeated parameterized SELECT query patterns\n"
              << '\n'
              << "Global options:\n"
              << "  -c, --config <path>   Path to the Worm configuration file\n"
              << "  --manifest <path>     Path to the Worm schema manifest\n"
              << "  --driver <driver>     Database driver (postgresql, mysql, sqlite, mssql)\n"
              << "  --host <host>         Database host\n"
              << "  --port <port>         Database port\n"
              << "  --database <name>     Database name or SQLite database path\n"
              << "  --username <name>     Database username\n"
              << "  --password-env <var>  Read the database password from an environment variable\n"
              << "  --format <format>     Output format (text, json)\n"
              << "  --verbose             Enable verbose output\n"
              << "  --no-color            Disable ANSI colors\n"
              << "  -h, --help            Show this help message\n"
              << "  -V, --version         Show Worm version\n"
              << '\n'
              << "Command options:\n"
              << "  --entity <name>       Select an entity (repeatable)\n"
              << "  --table <name>        Select a table (repeatable)\n"
              << "  --output <path>       Output directory for pull or SQL file for push\n"
              << "  --namespace <name>    Namespace for generated entities\n"
              << "  --name <name>         Explicit generated entity name\n"
              << "  --apply               Apply the generated plan\n"
              << "  --query <sql>         Analyze one observed SELECT query\n"
              << "  --file <path>         Analyze semicolon-separated SELECT queries from a file\n"
              << "  --max-executions <n>  Allow a query pattern to execute n times before reporting it\n"
              << '\n'
              << "Examples:\n"
              << "  worm check\n"
              << "  worm push\n"
              << "  worm push --apply\n"
              << "  worm pull\n"
              << "  worm diff\n"
              << "  worm --driver sqlite --database application.db inspect\n"
              << "  worm pull --apply\n"
              << "  worm push --entity User\n"
              << "  worm pull --table users\n"
              << "  worm n-plus-one --query \"SELECT * FROM posts WHERE user_id = 1\"\n"
              << "  worm n-plus-one --file query-log.sql --max-executions 1\n";
  }

  void printSystemVersion() noexcept
  {
#ifdef WORM_VERSION
    std::cout << "worm " << WORM_VERSION << '\n';
#else
    std::cout << "worm (unknown version)\n";
#endif
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
      } else if (argument.starts_with("--query=")) {
        command << "--query=<redacted>";
      } else {
        command << argument;
      }

      if ((argument == "--password" || argument == "--query") && index + 1 < argc) {
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
    case ExecutionStatus::IssuesDetected:
    case ExecutionStatus::DriftDetected:
      return 2;
    case ExecutionStatus::Blocked:
      return 3;
    }

    return 1;
  }

  void outputReport(const ExecutionReport& report, std::string_view format, std::ostream& out)
  {
    if (report.renderedOutput.has_value()) {
      out << *report.renderedOutput;
      if (report.renderedOutput->empty() || report.renderedOutput->back() != '\n') {
        out << '\n';
      }
      return;
    }

    if (report.metrics == nullptr) {
      throw InvalidCliArgumentException("Execution report has no metrics.");
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
      report = generator::push(invocation);
      break;
    case Commands::NPlusOne:
      report = database::verify(invocation);
      break;
    case Commands::Inspect:
      report = database::inspect(invocation);
      break;
    case Commands::Diff:
      report = database::diff(invocation);
      break;
    default:
      throw InvalidCliArgumentException("Command is unknown or not implemented.");
    }

    report.command = buildCommand(argc, argv);
    outputReport(report, invocation.global.format.value_or("text"), out);
    return exitCode(report.status);
  }
} // namespace worm::cli
