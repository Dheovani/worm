#include "validator.hpp"

#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "errors/empty-command-exception.hpp"
#include "errors/invalid-argument-exception.hpp"

namespace worm::cli
{
  namespace
  {
    inline constexpr std::string_view cppKeywords[] = {
      "alignas",
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

    [[noreturn]]
    void throwConfigurationError(const std::filesystem::path& path, std::size_t line, std::string_view message)
    {
      throw InvalidArgumentException("Invalid configuration file '" + path.string() + "' at line " +
                                     std::to_string(line) + ": " + std::string{message});
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

    void assignConfigurationValue(std::optional<std::string>& destination,
      std::string value,
      std::string_view key,
      const std::filesystem::path& path,
      std::size_t line)
    {
      if (destination.has_value()) {
        throwConfigurationError(path, line, "duplicate key '" + std::string{key} + "'");
      }

      destination = std::move(value);
    }

    void parseGeneratorValue(Configuration& configuration,
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
        throwConfigurationError(path, line, "unknown generator key '" + std::string{key} + "'");
      }
    }

    void parseDatabaseValue(Configuration& configuration,
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
        throwConfigurationError(path, line, "unknown database key '" + std::string{key} + "'");
      }
    }

    [[nodiscard]]
    Configuration parseConfiguration(const std::filesystem::path& path)
    {
      std::ifstream stream{path};
      if (!stream) {
        throw InvalidArgumentException("Unable to open configuration file '" + path.string() + "'.");
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
        } else {
          parseDatabaseValue(configuration, key, value, path, lineNumber);
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
    }

    void resolvePassword(GlobalArguments& arguments)
    {
      if (!arguments.passwordEnv.has_value()) {
        return;
      }

      if (arguments.passwordEnv->empty()) {
        throw InvalidArgumentException("Option '--password-env' cannot be empty.");
      }

      const char* password = std::getenv(arguments.passwordEnv->c_str());
      if (password == nullptr) {
        throw InvalidArgumentException(
          "Environment variable '" + *arguments.passwordEnv + "' referenced by '--password-env' is not defined.");
      }

      arguments.password = password;
    }

    [[nodiscard]]
    constexpr bool isCppKeyword(std::string_view value) noexcept
    {
      for (const std::string_view keyword : cppKeywords) {
        if (value == keyword)
          return true;
      }

      return false;
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
        throw InvalidArgumentException("Invalid C++ entity name '" + std::string{name} + "'.");
      }

      if (isCppKeyword(name)) {
        throw InvalidArgumentException("Entity name '" + std::string{name} + "' is a reserved C++ keyword.");
      }
    }

    void isValidNamespace(std::string_view value)
    {
      if (value.empty()) {
        throw InvalidArgumentException("Namespace cannot be empty.");
      }

      std::size_t begin = 0;

      while (begin < value.size()) {
        const auto end = value.find("::", begin);

        const auto component = value.substr(begin, end == std::string_view::npos ? value.size() - begin : end - begin);

        if (component.empty() || !isValidIdentifier(component)) {
          throw InvalidArgumentException("Invalid C++ namespace '" + std::string{value} + "'.");
        }

        if (end == std::string_view::npos)
          break;

        begin = end + 2;
      }
    }

    void validateCommand(Commands command)
    {
      if (command != Commands::Check && command != Commands::Push && command != Commands::Pull) {
        throw EmptyCommandException("No valid command given");
      }
    }

    void validateGlobalArguments(const GlobalArguments& args)
    {
      if (args.config.has_value() && args.config->empty()) {
        throw InvalidArgumentException("Option '--config' cannot be empty.");
      }

      if (args.manifest.has_value() && args.manifest->empty()) {
        throw InvalidArgumentException("Option '--manifest' cannot be empty.");
      }

      if (args.driver.has_value()) {
        if (args.driver->empty()) {
          throw InvalidArgumentException("Option '--driver' cannot be empty.");
        }

        if (!isSupportedDriver(*args.driver)) {
          throw InvalidArgumentException("Unsupported database driver '" + *args.driver + "'.");
        }
      }

      if (args.host.has_value() && args.host->empty()) {
        throw InvalidArgumentException("Option '--host' cannot be empty.");
      }

      if (args.port.has_value() && !isValidPort(*args.port)) {
        throw InvalidArgumentException("Option '--port' must be an integer between 1 and 65535.");
      }

      if (args.database.has_value() && args.database->empty()) {
        throw InvalidArgumentException("Option '--database' cannot be empty.");
      }

      if (args.username.has_value() && args.username->empty()) {
        throw InvalidArgumentException("Option '--username' cannot be empty.");
      }

      if (args.passwordEnv.has_value() && args.passwordEnv->empty()) {
        throw InvalidArgumentException("Option '--password-env' cannot be empty.");
      }

      if (args.format.has_value()) {
        if (args.format->empty()) {
          throw InvalidArgumentException("Option '--format' cannot be empty.");
        }

        if (!isSupportedFormat(*args.format)) {
          throw InvalidArgumentException("Unsupported output format '" + *args.format + "'.");
        }
      }
    }

    void validateCheckArguments(const CommandArguments& args)
    {
      if (args.output.has_value()) {
        throw InvalidArgumentException("Option '--output' is not valid for the 'check' command.");
      }

      if (args.namespaceName.has_value()) {
        throw InvalidArgumentException("Option '--namespace' is not valid for the 'check' command.");
      }

      if (args.name.has_value()) {
        throw InvalidArgumentException("Option '--name' is not valid for the 'check' command.");
      }

      if (args.apply) {
        throw InvalidArgumentException("Option '--apply' is not valid for the 'check' command.");
      }

      if (!args.entities.empty() && !args.tables.empty()) {
        throw InvalidArgumentException("Options '--entity' and '--table' cannot be used together "
                                       "for the 'check' command.");
      }
    }

    void validatePushArguments(const CommandArguments& args)
    {
      if (!args.tables.empty()) {
        throw InvalidArgumentException("Option '--table' is not valid for the 'push' command.");
      }

      if (args.output.has_value()) {
        throw InvalidArgumentException("Option '--output' is not valid for the 'push' command.");
      }

      if (args.namespaceName.has_value()) {
        throw InvalidArgumentException("Option '--namespace' is not valid for the 'push' command.");
      }

      if (args.name.has_value()) {
        throw InvalidArgumentException("Option '--name' is not valid for the 'push' command.");
      }
    }

    void validatePullArguments(const CommandArguments& args)
    {
      if (!args.entities.empty()) {
        throw InvalidArgumentException("Option '--entity' is not valid for the 'pull' command.");
      }

      if (args.name.has_value()) {
        if (args.tables.size() != 1) {
          throw InvalidArgumentException("Option '--name' requires exactly one table selected with '--table'.");
        }

        isValidEntityName(*args.name);
      }

      if (args.namespaceName.has_value()) {
        isValidNamespace(*args.namespaceName);
      }
    }
  } // namespace

  void validate(const Invocation& invocation)
  {
    validateCommand(invocation.command);
    validateGlobalArguments(invocation.global);

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
      throw InvalidArgumentException(
        "Unable to inspect configuration file '" + configurationPath.string() + "': " + error.message() + ".");
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
      throw InvalidArgumentException("Configuration file '" + configurationPath.string() + "' " + reason + ".");
    }

    if (!invocation.global.format.has_value()) {
      invocation.global.format = "text";
    }

    resolvePassword(invocation.global);
  }
} // namespace worm::cli
