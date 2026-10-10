#pragma once

#include <optional>
#include <string>
#include <vector>

namespace worm::cli
{
  enum class GlobalOptions
  {
    Config,
    Manifest,
    Driver,
    Host,
    Port,
    Database,
    Username,
    Password,
    TrustServerCertificate,
    Format,
    Verbose,
    NoColor
  };

  enum class Commands
  {
    Pull,
    Push,
    Check,
    NPlusOne,
    Inspect,
    Diff,
    Migrate,
    Doctor,
    Seed
  };

  enum class MigrationAction
  {
    Apply,
    Create,
    Validate,
    Status,
    Rollback
  };

  enum class DoctorCheck
  {
    Configuration,
    Driver,
    Connection,
    Version,
    Permissions
  };

  enum class CommandOptions
  {
    All,
    Entity,
    Table,
    Output,
    Schema,
    Namespace,
    Name,
    Apply,
    Query,
    File,
    Rows,
    MaxExecutions,
    Directory
  };

  struct GlobalArguments
  {
    std::optional<std::string> config;
    std::optional<std::string> manifest;
    std::optional<std::string> driver;
    std::optional<std::string> host;
    std::optional<std::string> port;
    std::optional<std::string> database;
    std::optional<std::string> username;
    std::optional<std::string> passwordEnv;
    std::optional<std::string> password;
    std::optional<std::string> format;
    bool trustServerCertificate{false};

    bool verbose{false};
    bool noColor{false};
  };

  struct CommandArguments
  {
    std::vector<std::string> entities;
    std::vector<std::string> tables;

    std::optional<std::string> schema;
    std::optional<std::string> output;
    std::optional<std::string> namespaceName;
    std::optional<std::string> name;
    std::optional<std::string> file;
    std::optional<std::string> rows;
    std::optional<std::string> query;
    std::optional<std::string> maxExecutions;
    std::optional<std::string> directory;

    bool all{false};
    bool apply{false};
  };

  struct Invocation
  {
    GlobalArguments global;
    Commands command;
    std::optional<MigrationAction> migrationAction;
    std::vector<DoctorCheck> doctorChecks;
    CommandArguments arguments;
  };

  [[nodiscard]]
  std::vector<std::string> listArguments(int argc, char** argv) noexcept;

  [[nodiscard]]
  bool showHelp(const std::vector<std::string>& args) noexcept;

  [[nodiscard]]
  bool showVersion(const std::vector<std::string>& args) noexcept;

  [[nodiscard]]
  Invocation parse(const std::vector<std::string>& args);

} // namespace worm::cli
