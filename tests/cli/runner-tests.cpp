#include <runner.hpp>

#include <iostream>
#include <memory>
#include <sstream>
#include <string>

namespace
{
  class Metrics final : public worm::cli::ExecutionMetrics
  {
  public:
    void writeText(std::ostream& out) const override
    {
      out << "text metrics\n";
    }

    void writeJson(std::ostream& out) const override
    {
      out << "{\"value\":1}";
    }
  };
} // namespace

int main()
{
  char executable[] = "worm";
  char command[] = "check";
  char* arguments[]{executable, command};
  char passwordOption[] = "--password";
  char password[] = "secret";
  char inlinePassword[] = "--password=other-secret";
  char queryOption[] = "--query";
  char query[] = "SELECT * FROM users WHERE token = 'secret-token'";
  char* passwordArguments[]{executable, passwordOption, password, command};
  char* inlinePasswordArguments[]{executable, inlinePassword, command};
  char* queryArguments[]{executable, command, queryOption, query};
  if (worm::cli::buildCommand(2, arguments) != "worm check" ||
      worm::cli::buildCommand(4, passwordArguments) != "worm --password <redacted> check" ||
      worm::cli::buildCommand(3, inlinePasswordArguments) != "worm --password=<redacted> check" ||
      worm::cli::buildCommand(4, queryArguments) != "worm check --query <redacted>" ||
      worm::cli::exitCode(worm::cli::ExecutionStatus::Success) != 0 ||
      worm::cli::exitCode(worm::cli::ExecutionStatus::Failed) != 1 ||
      worm::cli::exitCode(worm::cli::ExecutionStatus::IssuesDetected) != 2 ||
      worm::cli::exitCode(worm::cli::ExecutionStatus::DriftDetected) != 2 ||
      worm::cli::exitCode(worm::cli::ExecutionStatus::Blocked) != 3) {
    std::cerr << "Runner command or exit-code contract failed.\n";
    return 1;
  }

  const worm::cli::ExecutionReport report{
    .command = "worm check",
    .info = "Schema drift detected.",
    .status = worm::cli::ExecutionStatus::DriftDetected,
    .metrics = std::make_shared<Metrics>(),
  };

  std::ostringstream text;
  worm::cli::outputReport(report, "text", text);
  std::ostringstream json;
  worm::cli::outputReport(report, "json", json);

  if (text.str().find("Schema drift detected.") == std::string::npos ||
      text.str().find("text metrics") == std::string::npos ||
      json.str() != "{\"command\":\"worm check\",\"status\":\"drift\",\"info\":\"Schema drift "
                    "detected.\",\"metrics\":{\"value\":1}}\n") {
    std::cerr << "Runner output contract failed.\n";
    return 1;
  }

  return 0;
}
