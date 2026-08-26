#include "parser.hpp"

#include <cstddef>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "errors/command-overflow-exception.hpp"
#include "errors/duplicate-command-exception.hpp"
#include "errors/empty-command-exception.hpp"
#include "errors/missing-option-value-exception.hpp"
#include "errors/option-position-exception.hpp"
#include "errors/unknown-argument-exception.hpp"

namespace worm::cli
{
  namespace
  {
    inline constexpr std::string_view configCommand = "--config";
    inline constexpr std::string_view configShortCommand = "-c";
    inline constexpr std::string_view manifestCommand = "--manifest";
    inline constexpr std::string_view driverCommand = "--driver";
    inline constexpr std::string_view hostCommand = "--host";
    inline constexpr std::string_view portCommand = "--port";
    inline constexpr std::string_view databaseCommand = "--database";
    inline constexpr std::string_view usernameCommand = "--username";
    inline constexpr std::string_view passwordEnvCommand = "--password-env";
    inline constexpr std::string_view formatCommand = "--format";
    inline constexpr std::string_view verboseCommand = "--verbose";
    inline constexpr std::string_view noColorCommand = "--no-color";
    inline constexpr std::string_view helpCommand = "-h";
    inline constexpr std::string_view helpFullCommand = "--help";
    inline constexpr std::string_view versionCommand = "-V";
    inline constexpr std::string_view versionFullCommand = "--version";

    inline constexpr std::string_view pullCommand = "pull";
    inline constexpr std::string_view pushCommand = "push";
    inline constexpr std::string_view checkCommand = "check";

    inline constexpr std::string_view entityCommand = "--entity";
    inline constexpr std::string_view tableCommand = "--table";
    inline constexpr std::string_view outputCommand = "--output";
    inline constexpr std::string_view namespaceCommand = "--namespace";
    inline constexpr std::string_view nameCommand = "--name";
    inline constexpr std::string_view applyCommand = "--apply";

    [[nodiscard]]
    constexpr std::optional<GlobalOptions> parseGlobalOption(std::string_view opt) noexcept
    {
      using enum GlobalOptions;

      if (opt == configCommand || opt == configShortCommand)
        return Config;
      if (opt == manifestCommand)
        return Manifest;
      if (opt == driverCommand)
        return Driver;
      if (opt == hostCommand)
        return Host;
      if (opt == portCommand)
        return Port;
      if (opt == databaseCommand)
        return Database;
      if (opt == usernameCommand)
        return Username;
      if (opt == passwordEnvCommand)
        return Password;
      if (opt == formatCommand)
        return Format;
      if (opt == verboseCommand)
        return Verbose;
      if (opt == noColorCommand)
        return NoColor;

      return std::nullopt;
    }

    [[nodiscard]]
    constexpr std::optional<Commands> parseCommand(std::string_view cmd) noexcept
    {
      using enum Commands;

      if (cmd == pullCommand)
        return Pull;
      if (cmd == pushCommand)
        return Push;
      if (cmd == checkCommand)
        return Check;

      return std::nullopt;
    }

    [[nodiscard]]
    constexpr std::optional<CommandOptions> parseCommandOption(std::string_view opt) noexcept
    {
      using enum CommandOptions;

      if (opt == entityCommand)
        return Entity;
      if (opt == tableCommand)
        return Table;
      if (opt == outputCommand)
        return Output;
      if (opt == namespaceCommand)
        return Namespace;
      if (opt == nameCommand)
        return Name;
      if (opt == applyCommand)
        return Apply;

      return std::nullopt;
    }

    [[nodiscard]]
    constexpr bool requiresValue(GlobalOptions opt) noexcept
    {
      using enum GlobalOptions;
      return opt != Verbose && opt != NoColor;
    }

    [[nodiscard]]
    constexpr bool requiresValue(CommandOptions opt) noexcept
    {
      using enum CommandOptions;
      return opt != Apply;
    }

    [[nodiscard]]
    const std::string& consumeValue(const std::vector<std::string>& args, std::size_t& index, std::string_view option)
    {
      if (index + 1 >= args.size()) {
        throw MissingOptionValueException("Missing value for option '{}'.", option);
      }

      return args[++index];
    }

    void assignUnique(std::optional<std::string>& destination, std::string value, std::string_view option)
    {
      if (destination.has_value()) {
        throw DuplicateCommandException("Option '{}' was specified more than once.", option);
      }

      destination = std::move(value);
    }

    void assignGlobalOption(
      GlobalArguments& global, GlobalOptions option, std::optional<std::string> value, std::string_view token)
    {
      using enum GlobalOptions;

      switch (option) {
      case Config:
        assignUnique(global.config, std::move(value).value(), token);
        return;
      case Manifest:
        assignUnique(global.manifest, std::move(value).value(), token);
        return;
      case Driver:
        assignUnique(global.driver, std::move(value).value(), token);
        return;
      case Host:
        assignUnique(global.host, std::move(value).value(), token);
        return;
      case Port:
        assignUnique(global.port, std::move(value).value(), token);
        return;
      case Database:
        assignUnique(global.database, std::move(value).value(), token);
        return;
      case Username:
        assignUnique(global.username, std::move(value).value(), token);
        return;
      case Password:
        assignUnique(global.passwordEnv, std::move(value).value(), token);
        return;
      case Format:
        assignUnique(global.format, std::move(value).value(), token);
        return;
      case Verbose:
        if (global.verbose) {
          throw DuplicateCommandException("Option '--verbose' was specified more than once.");
        }
        global.verbose = true;
        return;
      case NoColor:
        if (global.noColor) {
          throw DuplicateCommandException("Option '--no-color' was specified more than once.");
        }
        global.noColor = true;
        return;
      }
    }

    void assignCommandOption(
      CommandArguments& arguments, CommandOptions option, std::optional<std::string> value, std::string_view token)
    {
      using enum CommandOptions;

      switch (option) {
      case Entity:
        arguments.entities.emplace_back(std::move(value).value());
        return;
      case Table:
        arguments.tables.emplace_back(std::move(value).value());
        return;
      case Output:
        assignUnique(arguments.output, std::move(value).value(), token);
        return;
      case Namespace:
        assignUnique(arguments.namespaceName, std::move(value).value(), token);
        return;
      case Name:
        assignUnique(arguments.name, std::move(value).value(), token);
        return;
      case Apply:
        if (arguments.apply) {
          throw DuplicateCommandException("Option '--apply' was specified more than once.");
        }
        arguments.apply = true;
        return;
      }
    }
  } // namespace

  std::vector<std::string> listArguments(int argc, char** argv) noexcept
  {
    if (argc <= 1 || argv == nullptr)
      return {};

    return {argv + 1, argv + argc};
  }

  bool showHelp(const std::vector<std::string>& args) noexcept
  {
    return args.size() == 1 && (args.front() == helpCommand || args.front() == helpFullCommand);
  }

  bool showVersion(const std::vector<std::string>& args) noexcept
  {
    return args.size() == 1 && (args.front() == versionCommand || args.front() == versionFullCommand);
  }

  Invocation parse(const std::vector<std::string>& args)
  {
    if (args.empty()) {
      throw EmptyCommandException("No command specified.");
    }

    GlobalArguments global;
    CommandArguments commandArguments;
    std::optional<Commands> command;

    for (std::size_t i = 0; i < args.size(); ++i) {
      const std::string_view token = args[i];

      if (!command.has_value()) {
        if (const auto globalOption = parseGlobalOption(token)) {
          std::optional<std::string> value;

          if (requiresValue(*globalOption)) {
            value = consumeValue(args, i, token);
          }

          assignGlobalOption(global, *globalOption, std::move(value), token);
          continue;
        }

        if (const auto parsedCommand = parseCommand(token)) {
          command = *parsedCommand;
          continue;
        }

        if (parseCommandOption(token).has_value()) {
          throw OptionPositionException("Command option '{}' appears before a command.", token);
        }

        throw UnknownArgumentException("Unknown argument '{}'.", token);
      }

      if (const auto secondCommand = parseCommand(token)) {
        static_cast<void>(secondCommand);
        throw CommandOverflowException("More than one command was specified. Unexpected command '{}'.", token);
      }

      if (parseGlobalOption(token).has_value()) {
        throw OptionPositionException("Global option '{}' must appear before the command.", token);
      }

      if (const auto commandOption = parseCommandOption(token)) {
        std::optional<std::string> value;

        if (requiresValue(*commandOption)) {
          value = consumeValue(args, i, token);
        }

        assignCommandOption(commandArguments, *commandOption, std::move(value), token);
        continue;
      }

      throw UnknownArgumentException("Unknown argument '{}'.", token);
    }

    if (!command.has_value()) {
      throw EmptyCommandException("No command specified.");
    }

    return Invocation{.global = std::move(global), .command = *command, .arguments = std::move(commandArguments)};
  }
} // namespace worm::cli
