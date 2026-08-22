#include <validator.hpp>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <errors/invalid-cli-argument-exception.hpp>

namespace
{
  namespace cli = worm::cli;

  class TemporaryDirectory
  {
  public:
    TemporaryDirectory()
      : path_(std::filesystem::temp_directory_path() / "worm-cli-resolver-tests")
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

    [[nodiscard]]
    const std::filesystem::path& path() const noexcept
    {
      return path_;
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

  class CurrentPathGuard
  {
  public:
    explicit CurrentPathGuard(const std::filesystem::path& path)
      : original_(std::filesystem::current_path())
    {
      std::filesystem::current_path(path);
    }

    ~CurrentPathGuard()
    {
      std::filesystem::current_path(original_);
    }

  private:
    std::filesystem::path original_;
  };

  void setEnvironment(const char* key, const char* value)
  {
#ifdef _WIN32
    _putenv_s(key, value);
#else
    setenv(key, value, 1);
#endif
  }

  void unsetEnvironment(const char* key)
  {
#ifdef _WIN32
    _putenv_s(key, "");
#else
    unsetenv(key);
#endif
  }

  [[nodiscard]]
  cli::Invocation invocation(cli::Commands command)
  {
    return cli::Invocation{.command = command};
  }

  bool resolvesExplicitConfiguration(const TemporaryDirectory& temporary)
  {
    setEnvironment("WORM_CLI_TEST_PASSWORD", "secret");
    const auto path = temporary.write("explicit.toml",
      "[generator]\n"
      "manifest = \"build/schema.json\"\n"
      "output = \"src/entities\"\n"
      "namespace = \"application::entities\"\n"
      "\n"
      "[database]\n"
      "driver = \"postgresql\"\n"
      "host = \"localhost\"\n"
      "port = 5432\n"
      "database = \"application\"\n"
      "username = \"postgres\"\n"
      "password_env = \"WORM_CLI_TEST_PASSWORD\"\n");

    auto resolved = invocation(cli::Commands::Pull);
    resolved.global.config = path.string();
    cli::resolve(resolved);
    unsetEnvironment("WORM_CLI_TEST_PASSWORD");

    return resolved.global.manifest == "build/schema.json" && resolved.global.driver == "postgresql" &&
           resolved.global.host == "localhost" && resolved.global.port == "5432" &&
           resolved.global.database == "application" && resolved.global.username == "postgres" &&
           resolved.global.password == "secret" && resolved.global.format == "text" &&
           resolved.arguments.output == "src/entities" && resolved.arguments.namespaceName == "application::entities";
  }

  bool preservesCommandLinePrecedence(const TemporaryDirectory& temporary)
  {
    const auto path = temporary.write("precedence.toml",
      "[generator]\n"
      "manifest = \"configured.json\"\n"
      "output = \"configured/entities\"\n"
      "namespace = \"configured\"\n"
      "\n"
      "[database]\n"
      "driver = \"postgresql\"\n"
      "database = \"configured\"\n");

    auto resolved = invocation(cli::Commands::Pull);
    resolved.global.config = path.string();
    resolved.global.manifest = "explicit.json";
    resolved.global.driver = "sqlite";
    resolved.global.database = "explicit.db";
    resolved.global.format = "json";
    resolved.arguments.output = "explicit/entities";
    resolved.arguments.namespaceName = "explicit";
    cli::resolve(resolved);

    return resolved.global.manifest == "explicit.json" && resolved.global.driver == "sqlite" &&
           resolved.global.database == "explicit.db" && resolved.global.format == "json" &&
           resolved.arguments.output == "explicit/entities" && resolved.arguments.namespaceName == "explicit";
  }

  bool resolvesPasswordEnvironmentFromCommandLine()
  {
    setEnvironment("WORM_CLI_COMMAND_PASSWORD", "command-secret");
    auto resolved = cli::parse({"--password-env", "WORM_CLI_COMMAND_PASSWORD", "check"});
    cli::resolve(resolved);
    unsetEnvironment("WORM_CLI_COMMAND_PASSWORD");

    return resolved.global.passwordEnv == "WORM_CLI_COMMAND_PASSWORD" && resolved.global.password == "command-secret";
  }

  bool discoversDefaultConfiguration(const TemporaryDirectory& temporary)
  {
    temporary.write("worm.toml",
      "[generator]\n"
      "manifest = \"default.json\"\n"
      "\n"
      "[database]\n"
      "driver = \"sqlite\"\n"
      "database = \"worm.db\"\n");

    CurrentPathGuard guard{temporary.path()};
    auto resolved = invocation(cli::Commands::Check);
    cli::resolve(resolved);

    return resolved.global.config == (temporary.path() / "worm.toml").string() &&
           resolved.global.manifest == "default.json" && resolved.global.driver == "sqlite" &&
           resolved.global.database == "worm.db";
  }

  bool ignoresPullOnlyConfigurationForOtherCommands(const TemporaryDirectory& temporary)
  {
    const auto path = temporary.write("scope.toml",
      "[generator]\n"
      "output = \"src/entities\"\n"
      "namespace = \"application::entities\"\n");

    auto resolved = invocation(cli::Commands::Push);
    resolved.global.config = path.string();
    cli::resolve(resolved);

    return !resolved.arguments.output.has_value() && !resolved.arguments.namespaceName.has_value();
  }

  bool rejectsMissingExplicitConfiguration(const TemporaryDirectory& temporary)
  {
    auto resolved = invocation(cli::Commands::Check);
    resolved.global.config = (temporary.path() / "missing.toml").string();

    try {
      cli::resolve(resolved);
    } catch (const cli::InvalidCliArgumentException&) {
      return true;
    }

    return false;
  }

  bool rejectsMissingPasswordEnvironment(const TemporaryDirectory& temporary)
  {
    unsetEnvironment("WORM_CLI_MISSING_PASSWORD");
    const auto path = temporary.write("password.toml",
      "[database]\n"
      "password_env = \"WORM_CLI_MISSING_PASSWORD\"\n");

    auto resolved = invocation(cli::Commands::Check);
    resolved.global.config = path.string();

    try {
      cli::resolve(resolved);
    } catch (const cli::InvalidCliArgumentException&) {
      return true;
    }

    return false;
  }

  bool rejectsMalformedConfiguration(const TemporaryDirectory& temporary)
  {
    const auto path = temporary.write("malformed.toml", "[database]\ndriver = postgresql\n");
    auto resolved = invocation(cli::Commands::Check);
    resolved.global.config = path.string();

    try {
      cli::resolve(resolved);
    } catch (const cli::InvalidCliArgumentException&) {
      return true;
    }

    return false;
  }
} // namespace

int main()
{
  try {
    const TemporaryDirectory temporary;
    bool valid = true;

    const auto verify = [&valid](bool result, std::string_view scenario) {
      if (!result) {
        std::cerr << "Failed resolver scenario: " << scenario << '\n';
        valid = false;
      }
    };

    verify(resolvesExplicitConfiguration(temporary), "explicit configuration");
    verify(preservesCommandLinePrecedence(temporary), "command-line precedence");
    verify(resolvesPasswordEnvironmentFromCommandLine(), "command-line password environment");
    verify(discoversDefaultConfiguration(temporary), "default configuration discovery");
    verify(ignoresPullOnlyConfigurationForOtherCommands(temporary), "command-specific configuration scope");
    verify(rejectsMissingExplicitConfiguration(temporary), "missing explicit configuration");
    verify(rejectsMissingPasswordEnvironment(temporary), "missing password environment");
    verify(rejectsMalformedConfiguration(temporary), "malformed configuration");

    return valid ? 0 : 1;
  } catch (const std::exception& error) {
    std::cerr << "Unexpected resolver test exception: " << error.what() << '\n';
    return 1;
  }
}
