#include "validator.hpp"

#include <core/query/statement.hpp>
#include <core/query/validator.hpp>

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "errors/empty-command-exception.hpp"
#include "errors/invalid-cli-argument-exception.hpp"
#include "helpers/file.hpp"

namespace worm::cli
{
  namespace
  {
    inline constexpr std::string_view cppKeywords[] = {"alignas",
      "alignof",
      "and",
      "and_eq",
      "asm",
      "auto",
      "bitand",
      "bitor",
      "bool",
      "break",
      "case",
      "catch",
      "char",
      "char8_t",
      "char16_t",
      "char32_t",
      "class",
      "compl",
      "concept",
      "const",
      "consteval",
      "constexpr",
      "constinit",
      "const_cast",
      "continue",
      "co_await",
      "co_return",
      "co_yield",
      "decltype",
      "default",
      "delete",
      "do",
      "double",
      "dynamic_cast",
      "else",
      "enum",
      "explicit",
      "export",
      "extern",
      "false",
      "float",
      "for",
      "friend",
      "goto",
      "if",
      "inline",
      "int",
      "long",
      "mutable",
      "namespace",
      "new",
      "noexcept",
      "not",
      "not_eq",
      "nullptr",
      "operator",
      "or",
      "or_eq",
      "private",
      "protected",
      "public",
      "register",
      "reinterpret_cast",
      "requires",
      "return",
      "short",
      "signed",
      "sizeof",
      "static",
      "static_assert",
      "static_cast",
      "struct",
      "switch",
      "template",
      "this",
      "thread_local",
      "throw",
      "true",
      "try",
      "typedef",
      "typeid",
      "typename",
      "union",
      "unsigned",
      "using",
      "virtual",
      "void",
      "volatile",
      "wchar_t",
      "while",
      "xor",
      "xor_eq"};

    [[nodiscard]]
    std::string_view trim(std::string_view value) noexcept
    {
      const auto first = value.find_first_not_of(" \t\r\n");
      if (first == std::string_view::npos) {
        return {};
      }

      const auto last = value.find_last_not_of(" \t\r\n");
      return value.substr(first, last - first + 1);
    }

    [[nodiscard]]
    std::string_view removeComment(std::string_view line) noexcept
    {
      bool quoted = false;
      bool escaped = false;

      for (std::size_t index = 0; index < line.size(); ++index) {
        const char character = line[index];

        if (escaped) {
          escaped = false;
          continue;
        }

        if (quoted && character == '\\') {
          escaped = true;
          continue;
        }

        if (character == '"') {
          quoted = !quoted;
          continue;
        }

        if (!quoted && character == '#') {
          return line.substr(0, index);
        }
      }

      return line;
    }

    template <typename... Args>
    [[noreturn]]
    void throwConfigurationError(
      const std::filesystem::path& path,
      std::size_t line,
      std::format_string<Args...> message,
      Args&&... args)
    {
      throw InvalidCliArgumentException(
        "Invalid configuration file '{}' at line {}: {}",
        path.string(),
        line,
        std::format(message, std::forward<Args>(args)...));
    }

    [[nodiscard]]
    std::string parseStringValue(std::string_view value, const std::filesystem::path& path, std::size_t line)
    {
      if (value.size() < 2 || value.front() != '"' || value.back() != '"') {
        throwConfigurationError(path, line, "expected a quoted string value");
      }

      std::string result;
      result.reserve(value.size() - 2);

      for (std::size_t index = 1; index + 1 < value.size(); ++index) {
        const char character = value[index];
        if (character != '\\') {
          result.push_back(character);
          continue;
        }

        if (++index + 1 >= value.size()) {
          throwConfigurationError(path, line, "incomplete escape sequence");
        }

        switch (value[index]) {
        case '"':
          result.push_back('"');
          break;
        case '\\':
          result.push_back('\\');
          break;
        case 'n':
          result.push_back('\n');
          break;
        case 'r':
          result.push_back('\r');
          break;
        case 't':
          result.push_back('\t');
          break;
        default:
          throwConfigurationError(path, line, "unsupported escape sequence");
        }
      }

      return result;
    }

    void assignConfigurationValue(
      std::optional<std::string>& destination,
      std::string value,
      std::string_view key,
      const std::filesystem::path& path,
      std::size_t line)
    {
      if (destination.has_value()) {
        throwConfigurationError(path, line, "duplicate key '{}'", key);
      }

      destination = std::move(value);
    }

    void parseGeneratorValue(
      Configuration& configuration,
      std::string_view key,
      std::string_view value,
      const std::filesystem::path& path,
      std::size_t line)
    {
      auto parsed = parseStringValue(value, path, line);

      if (key == "manifest") {
        assignConfigurationValue(configuration.manifest, std::move(parsed), key, path, line);
      } else if (key == "output") {
        assignConfigurationValue(configuration.output, std::move(parsed), key, path, line);
      } else if (key == "namespace") {
        assignConfigurationValue(configuration.namespaceName, std::move(parsed), key, path, line);
      } else {
        throwConfigurationError(path, line, "unknown generator key '{}'", key);
      }
    }

    void parseDatabaseValue(
      Configuration& configuration,
      std::string_view key,
      std::string_view value,
      const std::filesystem::path& path,
      std::size_t line)
    {
      if (key == "port") {
        assignConfigurationValue(configuration.port, std::string{value}, key, path, line);
        return;
      }

      auto parsed = parseStringValue(value, path, line);

      if (key == "driver") {
        assignConfigurationValue(configuration.driver, std::move(parsed), key, path, line);
      } else if (key == "host") {
        assignConfigurationValue(configuration.host, std::move(parsed), key, path, line);
      } else if (key == "database") {
        assignConfigurationValue(configuration.database, std::move(parsed), key, path, line);
      } else if (key == "username") {
        assignConfigurationValue(configuration.username, std::move(parsed), key, path, line);
      } else if (key == "password_env") {
        assignConfigurationValue(configuration.passwordEnv, std::move(parsed), key, path, line);
      } else {
        throwConfigurationError(path, line, "unknown database key '{}'", key);
      }
    }

    void parseMigrationValue(
      Configuration& configuration,
      std::string_view key,
      std::string_view value,
      const std::filesystem::path& path,
      std::size_t line)
    {
      auto parsed = parseStringValue(value, path, line);
      if (key == "directory") {
        assignConfigurationValue(configuration.migrationDirectory, std::move(parsed), key, path, line);
      } else {
        throwConfigurationError(path, line, "unknown migrations key '{}'", key);
      }
    }

    [[nodiscard]]
    Configuration parseConfiguration(const std::filesystem::path& path)
    {
      std::ifstream stream{path};
      if (!stream) {
        throw InvalidCliArgumentException("Unable to open configuration file '{}'.", path.string());
      }

      Configuration configuration;
      ConfigurationSection section = ConfigurationSection::None;
      std::string buffer;
      std::size_t lineNumber = 0;

      while (std::getline(stream, buffer)) {
        ++lineNumber;
        const std::string_view line = trim(removeComment(buffer));
        if (line.empty()) {
          continue;
        }

        if (line.front() == '[') {
          if (line == "[generator]") {
            section = ConfigurationSection::Generator;
          } else if (line == "[database]") {
            section = ConfigurationSection::Database;
          } else if (line == "[migrations]") {
            section = ConfigurationSection::Migrations;
          } else {
            throwConfigurationError(path, lineNumber, "unknown section");
          }
          continue;
        }

        if (section == ConfigurationSection::None) {
          throwConfigurationError(path, lineNumber, "key is not inside a supported section");
        }

        const auto delimiter = line.find('=');
        if (delimiter == std::string_view::npos) {
          throwConfigurationError(path, lineNumber, "expected 'key = value'");
        }

        const std::string_view key = trim(line.substr(0, delimiter));
        const std::string_view value = trim(line.substr(delimiter + 1));
        if (key.empty() || value.empty()) {
          throwConfigurationError(path, lineNumber, "key and value cannot be empty");
        }

        if (section == ConfigurationSection::Generator) {
          parseGeneratorValue(configuration, key, value, path, lineNumber);
        } else if (section == ConfigurationSection::Database) {
          parseDatabaseValue(configuration, key, value, path, lineNumber);
        } else {
          parseMigrationValue(configuration, key, value, path, lineNumber);
        }
      }

      return configuration;
    }

    template <typename Value>
    void assignMissing(std::optional<Value>& destination, const std::optional<Value>& source)
    {
      if (!destination.has_value() && source.has_value()) {
        destination = source;
      }
    }

    void applyConfiguration(Invocation& invocation, const Configuration& configuration)
    {
      assignMissing(invocation.global.manifest, configuration.manifest);
      assignMissing(invocation.global.driver, configuration.driver);
      assignMissing(invocation.global.host, configuration.host);
      assignMissing(invocation.global.port, configuration.port);
      assignMissing(invocation.global.database, configuration.database);
      assignMissing(invocation.global.username, configuration.username);
      assignMissing(invocation.global.passwordEnv, configuration.passwordEnv);

      if (invocation.command == Commands::Pull) {
        assignMissing(invocation.arguments.output, configuration.output);
        assignMissing(invocation.arguments.namespaceName, configuration.namespaceName);
      }

      if (invocation.command == Commands::Migrate) {
        assignMissing(invocation.arguments.directory, configuration.migrationDirectory);
      }
    }

    void resolvePassword(GlobalArguments& arguments)
    {
      if (!arguments.passwordEnv.has_value()) {
        return;
      }

      if (arguments.passwordEnv->empty()) {
        throw InvalidCliArgumentException("Option '--password-env' cannot be empty.");
      }

      const char* password = std::getenv(arguments.passwordEnv->c_str());
      if (password == nullptr) {
        throw InvalidCliArgumentException(
          "Environment variable '{}' referenced by '--password-env' is not defined.",
          *arguments.passwordEnv);
      }

      arguments.password = password;
    }

    [[nodiscard]]
    constexpr bool isValidIdentifier(std::string_view value) noexcept
    {
      if (value.empty())
        return false;

      const auto isAlpha = [](char c) constexpr { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); };

      const auto isDigit = [](char c) constexpr { return c >= '0' && c <= '9'; };

      if (!isAlpha(value.front()))
        return false;

      for (const char c : value.substr(1)) {
        if (!isAlpha(c) && !isDigit(c) && c != '_')
          return false;
      }

      return true;
    }

    [[nodiscard]]
    constexpr bool isSupportedDriver(std::string_view driver) noexcept
    {
      return driver == "postgresql" || driver == "mysql" || driver == "sqlite" || driver == "mssql";
    }

    [[nodiscard]]
    constexpr bool isSupportedFormat(std::string_view format) noexcept
    {
      return format == "text" || format == "json";
    }

    [[nodiscard]]
    bool isValidPort(std::string_view value) noexcept
    {
      if (value.empty())
        return false;

      unsigned int port = 0;

      const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), port);

      return ec == std::errc{} && ptr == value.data() + value.size() && port >= 1 && port <= 65535;
    }

    void isValidEntityName(std::string_view name)
    {
      if (!isValidIdentifier(name)) {
        throw InvalidCliArgumentException("Invalid C++ entity name '{}'.", name);
      }

      if (isCppKeyword(name)) {
        throw InvalidCliArgumentException("Entity name '{}' is a reserved C++ keyword.", name);
      }
    }

    void isValidNamespace(std::string_view value)
    {
      if (value.empty()) {
        throw InvalidCliArgumentException("Namespace cannot be empty.");
      }

      std::size_t begin = 0;

      while (begin < value.size()) {
        const auto end = value.find("::", begin);

        const auto component = value.substr(begin, end == std::string_view::npos ? value.size() - begin : end - begin);

        if (component.empty() || !isValidIdentifier(component)) {
          throw InvalidCliArgumentException("Invalid C++ namespace '{}'.", value);
        }

        if (end == std::string_view::npos)
          break;

        begin = end + 2;
      }
    }

    [[nodiscard]]
    bool isPositiveInteger(std::string_view value) noexcept
    {
      std::size_t result{};

      const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), result);

      return ec == std::errc{} && ptr == value.data() + value.size() && result > 0 &&
             result < std::numeric_limits<std::size_t>::max();
    }

    [[nodiscard]]
    bool fileContainsOnlySelectQueries(const std::filesystem::path& path)
    {
      std::ifstream file{path};

      if (!file) {
        return false;
      }

      const std::string content{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};

      const auto statements = worm::core::splitStatementQueries(content);

      if (statements.empty()) {
        return false;
      }

      return std::ranges::all_of(statements, [](const std::string& sql) { return worm::core::isSelect(sql); });
    }

    void validateGlobalArguments(const GlobalArguments& args)
    {
      if (args.config.has_value() && args.config->empty()) {
        throw InvalidCliArgumentException("Option '--config' cannot be empty.");
      }

      if (args.manifest.has_value() && args.manifest->empty()) {
        throw InvalidCliArgumentException("Option '--manifest' cannot be empty.");
      }

      if (args.driver.has_value()) {
        if (args.driver->empty()) {
          throw InvalidCliArgumentException("Option '--driver' cannot be empty.");
        }

        if (!isSupportedDriver(*args.driver)) {
          throw InvalidCliArgumentException("Unsupported database driver '{}'.", *args.driver);
        }
      }

      if (args.host.has_value() && args.host->empty()) {
        throw InvalidCliArgumentException("Option '--host' cannot be empty.");
      }

      if (args.port.has_value() && !isValidPort(*args.port)) {
        throw InvalidCliArgumentException("Option '--port' must be an integer between 1 and 65535.");
      }

      if (args.database.has_value() && args.database->empty()) {
        throw InvalidCliArgumentException("Option '--database' cannot be empty.");
      }

      if (args.username.has_value() && args.username->empty()) {
        throw InvalidCliArgumentException("Option '--username' cannot be empty.");
      }

      if (args.passwordEnv.has_value() && args.passwordEnv->empty()) {
        throw InvalidCliArgumentException("Option '--password-env' cannot be empty.");
      }

      if (args.format.has_value()) {
        if (args.format->empty()) {
          throw InvalidCliArgumentException("Option '--format' cannot be empty.");
        }

        if (!isSupportedFormat(*args.format)) {
          throw InvalidCliArgumentException("Unsupported output format '{}'.", *args.format);
        }
      }
    }

    void validateCheckArguments(const CommandArguments& args)
    {
      if (args.output.has_value()) {
        throw InvalidCliArgumentException("Option '--output' is not valid for the 'check' command.");
      }

      if (args.namespaceName.has_value()) {
        throw InvalidCliArgumentException("Option '--namespace' is not valid for the 'check' command.");
      }

      if (args.name.has_value()) {
        throw InvalidCliArgumentException("Option '--name' is not valid for the 'check' command.");
      }

      if (args.apply) {
        throw InvalidCliArgumentException("Option '--apply' is not valid for the 'check' command.");
      }

      if (args.query.has_value() || args.file.has_value() || args.maxExecutions.has_value()) {
        throw InvalidCliArgumentException("N+1 options are only valid for the 'n-plus-one' command.");
      }

      if (!args.entities.empty() && !args.tables.empty()) {
        throw InvalidCliArgumentException(
          "Options '--entity' and '--table' cannot be used together "
          "for the 'check' command.");
      }
    }

    void validatePushArguments(const CommandArguments& args)
    {
      if (!args.tables.empty()) {
        throw InvalidCliArgumentException("Option '--table' is not valid for the 'push' command.");
      }

      if (args.output.has_value() && args.output->empty()) {
        throw InvalidCliArgumentException("Option '--output' cannot be empty.");
      }

      if (args.output.has_value() && args.apply) {
        throw InvalidCliArgumentException("Options '--output' and '--apply' cannot be used together for 'push'.");
      }

      if (args.namespaceName.has_value()) {
        throw InvalidCliArgumentException("Option '--namespace' is not valid for the 'push' command.");
      }

      if (args.name.has_value()) {
        throw InvalidCliArgumentException("Option '--name' is not valid for the 'push' command.");
      }

      if (args.query.has_value() || args.file.has_value() || args.maxExecutions.has_value()) {
        throw InvalidCliArgumentException("N+1 options are only valid for the 'n-plus-one' command.");
      }
    }

    void validatePullArguments(const CommandArguments& args)
    {
      if (!args.entities.empty()) {
        throw InvalidCliArgumentException("Option '--entity' is not valid for the 'pull' command.");
      }

      if (args.output.has_value() && args.output->empty()) {
        throw InvalidCliArgumentException("Option '--output' cannot be empty.");
      }

      if (args.name.has_value()) {
        if (args.tables.size() != 1) {
          throw InvalidCliArgumentException("Option '--name' requires exactly one table selected with '--table'.");
        }

        isValidEntityName(*args.name);
      }

      if (args.namespaceName.has_value()) {
        isValidNamespace(*args.namespaceName);
      }

      if (args.query.has_value() || args.file.has_value() || args.maxExecutions.has_value()) {
        throw InvalidCliArgumentException("N+1 options are only valid for the 'n-plus-one' command.");
      }
    }

    void validateNPlusOneArguments(const CommandArguments& args)
    {
      if (args.file.has_value() == args.query.has_value()) {
        throw InvalidCliArgumentException(
          "Exactly one of '--query' or '--file' must be provided for the 'n-plus-one' command.");
      }

      if (!args.entities.empty() || !args.tables.empty() || args.output.has_value() || args.namespaceName.has_value() ||
          args.name.has_value() || args.apply) {
        throw InvalidCliArgumentException("Generator options are not valid for the 'n-plus-one' command.");
      }

      if (args.query.has_value()) {
        if (args.query->empty()) {
          throw InvalidCliArgumentException("Option '--query' cannot be empty.");
        }

        const auto statements = worm::core::splitStatementQueries(*args.query);
        if (statements.size() != 1 || !worm::core::isSelect(statements.front())) {
          throw InvalidCliArgumentException("Option '--query' accepts exactly one read-only query.");
        }
      }

      if (args.file.has_value()) {
        if (args.file->empty()) {
          throw InvalidCliArgumentException("Option '--file' cannot be empty.");
        }

        if (!fileExists(args.file.value())) {
          throw InvalidCliArgumentException("Provided file does not exist.");
        }

        if (!fileHasContent(args.file.value())) {
          throw InvalidCliArgumentException("Provided file does not have any content.");
        }

        if (!fileContainsOnlySelectQueries(args.file.value())) {
          throw InvalidCliArgumentException("Option '--file' accepts only read-only query files.");
        }
      }

      if (args.maxExecutions.has_value()) {
        if (!isPositiveInteger(*args.maxExecutions)) {
          throw InvalidCliArgumentException("Option '--max-executions' must be a positive integer.");
        }
      }
    }

    void validateInspectArguments(const CommandArguments& args)
    {
      if (args.output.has_value() || !args.entities.empty() || !args.tables.empty() || args.namespaceName.has_value() ||
          args.name.has_value() || args.apply) {
        throw InvalidCliArgumentException("Generator options are not valid for the 'inspect' command.");
      }

      if (args.query.has_value() || args.file.has_value() || args.maxExecutions.has_value()) {
        throw InvalidCliArgumentException("N+1 options are only valid for the 'n-plus-one' command.");
      }
    }

    void validateDiffArguments(const CommandArguments& args)
    {
      if (args.output.has_value() || !args.entities.empty() || !args.tables.empty() || args.namespaceName.has_value() ||
          args.name.has_value() || args.apply) {
        throw InvalidCliArgumentException("Generator options are not valid for the 'diff' command.");
      }

      if (args.query.has_value() || args.file.has_value() || args.maxExecutions.has_value()) {
        throw InvalidCliArgumentException("N+1 options are only valid for the 'n-plus-one' command.");
      }
    }

    void validateMigrateArguments(const Invocation& invocation)
    {
      const CommandArguments& args = invocation.arguments;
      if (!invocation.migrationAction.has_value()) {
        throw InvalidCliArgumentException(
          "The 'migrate' command requires a subcommand. Currently supported: validate.");
      }

      if (args.directory.has_value() && args.directory->empty()) {
        throw InvalidCliArgumentException("Option '--directory' cannot be empty.");
      }

      if (args.output.has_value() || !args.entities.empty() || !args.tables.empty() || args.namespaceName.has_value() ||
          args.name.has_value() || args.apply || args.query.has_value() || args.file.has_value() ||
          args.maxExecutions.has_value()) {
        throw InvalidCliArgumentException("Only '--directory' is valid for the 'migrate validate' command.");
      }
    }
  } // namespace

  bool isCppKeyword(std::string_view value) noexcept
  {
    for (const std::string_view keyword : cppKeywords) {
      if (value == keyword)
        return true;
    }

    return false;
  }

  void validate(const Invocation& invocation)
  {
    validateGlobalArguments(invocation.global);

    if (invocation.command != Commands::Migrate && invocation.arguments.directory.has_value()) {
      throw InvalidCliArgumentException("Option '--directory' is only valid for the 'migrate' command.");
    }

    switch (invocation.command) {
    case Commands::Check:
      validateCheckArguments(invocation.arguments);
      break;
    case Commands::Push:
      validatePushArguments(invocation.arguments);
      break;
    case Commands::Pull:
      validatePullArguments(invocation.arguments);
      break;
    case Commands::NPlusOne:
      validateNPlusOneArguments(invocation.arguments);
      break;
    case Commands::Inspect:
      validateInspectArguments(invocation.arguments);
      break;
    case Commands::Diff:
      validateDiffArguments(invocation.arguments);
      break;
    case Commands::Migrate:
      validateMigrateArguments(invocation);
      break;
    default:
      throw EmptyCommandException("No valid command given");
    }
  }

  void resolve(Invocation& invocation)
  {
    std::filesystem::path configurationPath;
    bool explicitlyConfigured = false;

    if (invocation.global.config.has_value()) {
      configurationPath = *invocation.global.config;
      explicitlyConfigured = true;
    } else {
      configurationPath = std::filesystem::current_path() / "worm.toml";
    }

    std::error_code error;
    const auto configurationStatus = std::filesystem::status(configurationPath, error);
    if (error.default_error_condition() == std::errc::no_such_file_or_directory) {
      error.clear();
    }

    if (error) {
      throw InvalidCliArgumentException(
        "Unable to inspect configuration file '{}': {}.",
        configurationPath.string(),
        error.message());
    }

    const bool configurationExists = std::filesystem::is_regular_file(configurationStatus);
    if (configurationExists) {
      const Configuration configuration = parseConfiguration(configurationPath);
      applyConfiguration(invocation, configuration);

      if (!explicitlyConfigured) {
        invocation.global.config = configurationPath.string();
      }
    } else if (explicitlyConfigured) {
      const std::string reason =
        std::filesystem::exists(configurationStatus) ? "is not a regular file" : "does not exist";
      throw InvalidCliArgumentException("Configuration file '{}' {}.", configurationPath.string(), reason);
    }

    if (!invocation.global.format.has_value()) {
      invocation.global.format = "text";
    }

    resolvePassword(invocation.global);
  }
} // namespace worm::cli
