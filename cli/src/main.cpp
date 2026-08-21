#include <cstdlib>
#include <exception>
#include <iostream>

#include "errors/worm-cli-exception.hpp"
#include "parser.hpp"
#include "runner.hpp"
#include "validator.hpp"

namespace cli = worm::cli;

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
    cli::resolve(invocation);
    cli::validate(invocation);
    return cli::execute(invocation, argc, argv, std::cout);
  } catch (const cli::WormCliException& ex) {
    std::cerr << ex.what() << '\n';
    return EXIT_FAILURE;
  } catch (const std::exception& ex) {
    std::cerr << "Unexpected error: " << ex.what() << '\n';
    return EXIT_FAILURE;
  }
}
