#define WORM_FAILURE 2

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>

#include <utils/logger.hpp>

#include "errors/worm-cli-exception.hpp"
#include "parser.hpp"
#include "runner.hpp"
#include "validator.hpp"

namespace cli = worm::cli;

namespace
{
  [[nodiscard]]
  constexpr std::string_view commandName(cli::Commands command) noexcept
  {
    switch (command) {
    case cli::Commands::Pull:
      return "pull";
    case cli::Commands::Push:
      return "push";
    case cli::Commands::Check:
      return "check";
    case cli::Commands::NPlusOne:
      return "n-plus-one";
    case cli::Commands::Inspect:
      return "inspect";
    case cli::Commands::Diff:
      return "diff";
    case cli::Commands::Migrate:
      return "migrate";
    case cli::Commands::Doctor:
      return "doctor";
    case cli::Commands::Seed:
      return "seed";
    }

    return "unknown";
  }
} // namespace

int main(int argc, char** argv)
{
  const auto args = cli::listArguments(argc, argv);

  if (args.empty() || cli::showHelp(args)) {
    cli::printUsage();
    return EXIT_SUCCESS;
  }

  if (cli::showVersion(args)) {
    cli::printSystemVersion();
    return EXIT_SUCCESS;
  }

  try {
    cli::Invocation invocation = cli::parse(args);
    worm::logger.setMinimumLevel(invocation.global.verbose ? worm::LogLevel::Trace : worm::LogLevel::Warning);
    worm::logger.log(
      worm::LogLevel::Debug,
      "Command-line arguments parsed.",
      {
        {"command", std::string{commandName(invocation.command)}},
        {"argument_count", std::to_string(args.size())},
      });

    cli::resolve(invocation);
    worm::logger.log(
      worm::LogLevel::Debug,
      "Configuration resolved.",
      {
        {"command", std::string{commandName(invocation.command)}},
        {"driver", invocation.global.driver.value_or("default")},
        {"format", invocation.global.format.value_or("text")},
      });

    worm::logger.log(
      worm::LogLevel::Trace,
      "Command validation started.",
      {
        {"command", std::string{commandName(invocation.command)}},
      });
    cli::validate(invocation);
    worm::logger.log(
      worm::LogLevel::Info,
      "Command execution started.",
      {
        {"command", std::string{commandName(invocation.command)}},
      });

    const int result = cli::execute(invocation, argc, argv, std::cout);
    worm::logger.log(
      worm::LogLevel::Info,
      "Command execution finished.",
      {
        {"command", std::string{commandName(invocation.command)}},
        {"exit_code", std::to_string(result)},
      });
    return result;
  } catch (const cli::WormCliException& ex) {
    worm::logger.error(ex);
    std::cerr << ex.what() << '\n';
    return WORM_FAILURE;
  } catch (const std::exception& ex) {
    worm::logger.error(ex);
    std::cerr << "Unexpected error: " << ex.what() << '\n';
    return EXIT_FAILURE;
  }
}
