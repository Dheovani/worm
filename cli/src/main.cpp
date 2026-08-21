#include <cstdlib>
#include <iostream>

#include "parser.hpp"
#include "validator.hpp"
#include "errors/worm-cli-exception.hpp"

namespace cli = worm::cli;

int main(int argc, char **argv)
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
    // TODO: Execute the requested command.
    static_cast<void>(invocation);
  } catch (const cli::WormCliException &ex) {
    std::cerr << ex.what() << std::endl;
    return EXIT_FAILURE;
  } catch (...) {
    std::cerr << "Unknow exception" << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
