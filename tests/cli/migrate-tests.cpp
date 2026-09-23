#include <database/migrate.hpp>
#include <errors/invalid-cli-argument-exception.hpp>
#include <helpers/migration/migration-file.hpp>
#include <parser.hpp>
#include <validator.hpp>

#include <core/model/migration-artifact.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
  class TemporaryDirectory
  {
  public:
    TemporaryDirectory()
      : path_(std::filesystem::temp_directory_path() / "worm-cli-migrate-tests")
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

  private:
    std::filesystem::path path_;
  };

  template <typename Action>
  [[nodiscard]]
  bool rejectsArguments(Action&& action)
  {
    try {
      std::forward<Action>(action)();
    } catch (const worm::cli::InvalidCliArgumentException&) {
      return true;
    }
    return false;
  }

  [[nodiscard]]
  worm::core::MigrationArtifact migrationArtifact()
  {
    using worm::core::MigrationRisk;
    return worm::core::makeMigrationArtifact(
      "20260923120000",
      "create-users",
      "postgresql",
      {
        {"Create users", "CREATE TABLE users (id bigint)", MigrationRisk::Safe},
        {"Add legacy column", "ALTER TABLE users ADD COLUMN legacy text", MigrationRisk::Ambiguous},
        {"Drop legacy column", "ALTER TABLE users DROP COLUMN legacy", MigrationRisk::Destructive},
      },
      std::vector<worm::core::MigrationStatement>{
        {"Drop users", "DROP TABLE users", MigrationRisk::Destructive},
      });
  }
} // namespace

int main()
{
  const TemporaryDirectory temporary;
  const auto artifact = migrationArtifact();
  worm::cli::migration::saveMigrationArtifact(temporary.path() / "20260923120000_create-users.worm.json", artifact);

  worm::cli::Invocation invocation =
    worm::cli::parse({"migrate", "validate", "--directory", temporary.path().string()});
  worm::cli::validate(invocation);
  const worm::cli::ExecutionReport report = worm::cli::database::migrate(invocation);
  const auto metrics = std::dynamic_pointer_cast<const worm::cli::database::MigrationValidateMetrics>(report.metrics);

  if (invocation.command != worm::cli::Commands::Migrate ||
      invocation.migrationAction != worm::cli::MigrationAction::Validate ||
      report.status != worm::cli::ExecutionStatus::Success || metrics == nullptr || metrics->migrations != 1 ||
      metrics->statements != 3 || metrics->safeStatements != 1 || metrics->ambiguousStatements != 1 ||
      metrics->destructiveStatements != 1 || metrics->reversibleMigrations != 1) {
    std::cerr << "Migrate validate did not parse or summarize the migration catalog correctly.\n";
    return 1;
  }

  const std::filesystem::path configuration = temporary.path() / "worm.toml";
  {
    std::ofstream stream{configuration};
    stream << "[migrations]\n"
              "directory = \""
           << temporary.path().generic_string() << "\"\n";
  }

  worm::cli::Invocation configured = worm::cli::parse({"--config", configuration.string(), "migrate", "validate"});
  worm::cli::resolve(configured);
  worm::cli::validate(configured);
  if (configured.arguments.directory != temporary.path().generic_string()) {
    std::cerr << "Migrate validate did not resolve its configured migration directory.\n";
    return 1;
  }

  if (!rejectsArguments([] {
        const auto missingAction = worm::cli::parse({"migrate"});
        worm::cli::validate(missingAction);
      }) ||
      !rejectsArguments([] {
        const auto invalidOption = worm::cli::parse({"migrate", "validate", "--apply"});
        worm::cli::validate(invalidOption);
      }) ||
      !rejectsArguments([] {
        const auto misplacedDirectory = worm::cli::parse({"check", "--directory", "migrations"});
        worm::cli::validate(misplacedDirectory);
      })) {
    std::cerr << "Migrate validate accepted a missing action or an option outside its contract.\n";
    return 1;
  }

  return 0;
}
