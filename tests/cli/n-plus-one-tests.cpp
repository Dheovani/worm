#include <database/n-plus-one.hpp>
#include <parser.hpp>
#include <runner.hpp>
#include <validator.hpp>

#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

#include <errors/invalid-cli-argument-exception.hpp>

namespace
{
  namespace cli = worm::cli;

  class TemporaryDirectory
  {
  public:
    TemporaryDirectory()
      : path_(std::filesystem::temp_directory_path() / "worm-cli-n-plus-one-tests")
    {
      std::error_code error;
      std::filesystem::remove_all(path_, error);
      std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory()
    {
      std::error_code error;
      std::filesystem::remove_all(path_, error);
    }

    std::filesystem::path write(std::string_view name, std::string_view contents) const
    {
      const auto file = path_ / name;
      std::ofstream stream{file};
      stream << contents;
      return file;
    }

  private:
    std::filesystem::path path_;
  };

  [[nodiscard]]
  bool rejects(const std::vector<std::string>& arguments)
  {
    try {
      const auto invocation = cli::parse(arguments);
      cli::validate(invocation);
    } catch (const cli::InvalidCliArgumentException&) {
      return true;
    } catch (const std::exception&) {
      return true;
    }

    return false;
  }

  [[nodiscard]]
  bool parsesCommandOptions()
  {
    const auto invocation =
      cli::parse({"n-plus-one", "--query", "SELECT * FROM posts WHERE user_id = 1", "--max-executions", "3"});

    return invocation.command == cli::Commands::NPlusOne &&
           invocation.arguments.query == "SELECT * FROM posts WHERE user_id = 1" &&
           invocation.arguments.maxExecutions == "3" && !invocation.arguments.file.has_value();
  }

  [[nodiscard]]
  bool validatesExclusiveReadOnlySource(const TemporaryDirectory& temporary)
  {
    const auto validFile = temporary.write("valid.sql", "SELECT * FROM users; SELECT * FROM posts;");
    const auto mutationFile = temporary.write("mutation.sql", "SELECT * FROM users; DELETE FROM users;");

    const auto validInvocation = cli::parse({"n-plus-one", "--file", validFile.string()});
    cli::validate(validInvocation);

    return rejects({"n-plus-one"}) && rejects({"n-plus-one", "--query", "SELECT 1", "--file", validFile.string()}) &&
           rejects({"n-plus-one", "--query", "DELETE FROM users"}) &&
           rejects({"n-plus-one", "--query", "SELECT 1; DELETE FROM users"}) &&
           rejects({"n-plus-one", "--file", mutationFile.string()}) &&
           rejects({"n-plus-one", "--query", "SELECT 1", "--max-executions", "0"}) &&
           rejects({"n-plus-one", "--query", "SELECT 1", "--max-executions", "many"});
  }

  [[nodiscard]]
  bool analyzesRepeatedQueriesWithoutExposingValues(const TemporaryDirectory& temporary)
  {
    const auto file = temporary.write(
      "query-log.sql",
      "SELECT * FROM posts WHERE user_id = 104729;\n"
      "SELECT * FROM posts WHERE user_id = 1299709;\n");
    const auto invocation = cli::parse({"--format", "json", "n-plus-one", "--file", file.string()});
    cli::validate(invocation);

    auto report = cli::database::verifyNPlusOne(invocation);
    report.command = "worm n-plus-one --file query-log.sql";
    const auto metrics = std::dynamic_pointer_cast<const cli::database::NPlusOneMetrics>(report.metrics);

    std::ostringstream output;
    cli::outputReport(report, "json", output);
    const auto json = output.str();

    return report.status == cli::ExecutionStatus::IssuesDetected && metrics != nullptr &&
           metrics->queriesDiscovered == 2 && metrics->queriesAnalyzed == 2 && metrics->queryPatterns == 1 &&
           metrics->repeatedPatterns == 1 && metrics->potentialNPlusOnePatterns == 1 && metrics->affectedQueries == 2 &&
           json.find("select * from posts where user_id = ?") != std::string::npos &&
           json.find("104729") == std::string::npos && json.find("1299709") == std::string::npos;
  }

  [[nodiscard]]
  bool respectsMaximumExecutions(const TemporaryDirectory& temporary)
  {
    const auto file = temporary.write(
      "allowed-query-log.sql",
      "SELECT * FROM posts WHERE user_id = 1;\n"
      "SELECT * FROM posts WHERE user_id = 2;\n");
    const auto invocation = cli::parse({"n-plus-one", "--file", file.string(), "--max-executions", "2"});
    cli::validate(invocation);

    const auto report = cli::database::verifyNPlusOne(invocation);
    const auto metrics = std::dynamic_pointer_cast<const cli::database::NPlusOneMetrics>(report.metrics);

    return report.status == cli::ExecutionStatus::Success && metrics != nullptr && metrics->repeatedPatterns == 1 &&
           metrics->potentialNPlusOnePatterns == 0;
  }

  [[nodiscard]]
  bool analyzesSingleQueryPredictably()
  {
    const auto invocation = cli::parse({"n-plus-one", "--query", "SELECT * FROM users WHERE id = 7"});
    cli::validate(invocation);

    const auto report = cli::database::verifyNPlusOne(invocation);
    const auto metrics = std::dynamic_pointer_cast<const cli::database::NPlusOneMetrics>(report.metrics);

    return report.status == cli::ExecutionStatus::Success && metrics != nullptr && metrics->queriesDiscovered == 1 &&
           metrics->queriesAnalyzed == 1 && metrics->queryPatterns == 1 && metrics->potentialNPlusOnePatterns == 0;
  }
} // namespace

int main()
{
  try {
    const TemporaryDirectory temporary;
    bool valid = true;

    const auto verify = [&valid](bool result, std::string_view scenario) {
      if (!result) {
        std::cerr << "Failed N+1 CLI scenario: " << scenario << '\n';
        valid = false;
      }
    };

    verify(parsesCommandOptions(), "command parsing");
    verify(validatesExclusiveReadOnlySource(temporary), "source validation");
    verify(analyzesRepeatedQueriesWithoutExposingValues(temporary), "repeated-query analysis");
    verify(respectsMaximumExecutions(temporary), "maximum executions");
    verify(analyzesSingleQueryPredictably(), "single-query analysis");

    return valid ? 0 : 1;
  } catch (const std::exception& error) {
    std::cerr << "Unexpected N+1 CLI test exception: " << error.what() << '\n';
    return 1;
  }
}
