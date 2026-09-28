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
#include <sstream>
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

  [[nodiscard]]
  worm::core::MigrationArtifact statusArtifact(std::string id, std::string name)
  {
    return worm::core::makeMigrationArtifact(
      std::move(id),
      std::move(name),
      "postgresql",
      {{"Apply migration", "SELECT 1", worm::core::MigrationRisk::Safe}});
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

  const std::filesystem::path statusDirectory = temporary.path() / "status";
  std::filesystem::create_directories(statusDirectory);
  const auto appliedArtifact = statusArtifact("20260923120001", "applied");
  const auto pendingArtifact = statusArtifact("20260923120002", "pending");
  const auto failedArtifact = statusArtifact("20260923120003", "failed");
  const auto rolledBackArtifact = statusArtifact("20260923120004", "rolled-back");
  const auto divergentArtifact = statusArtifact("20260923120005", "divergent");
  for (const auto* statusMigration :
    {&appliedArtifact, &pendingArtifact, &failedArtifact, &rolledBackArtifact, &divergentArtifact}) {
    worm::cli::migration::saveMigrationArtifact(
      statusDirectory / (statusMigration->id() + "_" + statusMigration->name() + ".worm.json"),
      *statusMigration);
  }

  const worm::cli::migration::MigrationCatalog statusCatalog =
    worm::cli::migration::discoverMigrationArtifacts(statusDirectory);
  const worm::core::MigrationHistory statusHistory{{
    {
      .id = appliedArtifact.id(),
      .name = appliedArtifact.name(),
      .checksum = appliedArtifact.checksum(),
      .state = worm::core::MigrationState::Applied,
    },
    {
      .id = failedArtifact.id(),
      .name = failedArtifact.name(),
      .checksum = failedArtifact.checksum(),
      .state = worm::core::MigrationState::Failed,
    },
    {
      .id = rolledBackArtifact.id(),
      .name = rolledBackArtifact.name(),
      .checksum = rolledBackArtifact.checksum(),
      .state = worm::core::MigrationState::RolledBack,
    },
    {
      .id = divergentArtifact.id(),
      .name = divergentArtifact.name(),
      .checksum = "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      .state = worm::core::MigrationState::Applied,
    },
    {
      .id = "20260923120006",
      .name = "missing",
      .checksum = "sha256:bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
      .state = worm::core::MigrationState::Applied,
    },
  }};
  const worm::cli::ExecutionReport statusReport = worm::cli::database::migrationStatus(statusCatalog, statusHistory);
  const auto statusMetrics =
    std::dynamic_pointer_cast<const worm::cli::database::MigrationStatusMetrics>(statusReport.metrics);
  if (statusReport.status != worm::cli::ExecutionStatus::DriftDetected || statusMetrics == nullptr ||
      statusMetrics->migrations != 6 || statusMetrics->appliedMigrations != 1 ||
      statusMetrics->pendingMigrations != 2 || statusMetrics->failedMigrations != 1 ||
      statusMetrics->missingMigrations != 1 || statusMetrics->checksumDivergentMigrations != 1) {
    std::cerr << "Migrate status did not classify local and historical migrations correctly.\n";
    return 1;
  }

  std::ostringstream statusText;
  std::ostringstream statusJson;
  statusMetrics->writeText(statusText);
  statusMetrics->writeJson(statusJson);
  if (statusText.str().find("Checksum divergences") == std::string::npos ||
      statusJson.str().find("\"checksumDivergentMigrations\":1") == std::string::npos) {
    std::cerr << "Migrate status did not render checksum divergence metrics.\n";
    return 1;
  }

  const worm::core::MigrationHistory failedOnlyHistory{{
    {
      .id = failedArtifact.id(),
      .name = failedArtifact.name(),
      .checksum = failedArtifact.checksum(),
      .state = worm::core::MigrationState::Failed,
    },
  }};
  if (worm::cli::database::migrationStatus(statusCatalog, failedOnlyHistory).status !=
      worm::cli::ExecutionStatus::IssuesDetected) {
    std::cerr << "Migrate status did not report matching failed migrations as issues.\n";
    return 1;
  }

  worm::cli::Invocation statusInvocation =
    worm::cli::parse({"migrate", "status", "--directory", statusDirectory.string()});
  worm::cli::validate(statusInvocation);
  if (statusInvocation.migrationAction != worm::cli::MigrationAction::Status) {
    std::cerr << "Migrate status was not parsed as a migration subcommand.\n";
    return 1;
  }

  worm::cli::Invocation createInvocation =
    worm::cli::parse({"migrate", "create", "--name", "create-users", "--directory", statusDirectory.string()});
  worm::cli::validate(createInvocation);
  if (createInvocation.migrationAction != worm::cli::MigrationAction::Create ||
      createInvocation.arguments.name != "create-users") {
    std::cerr << "Migrate create was not parsed with its migration name.\n";
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
        const auto invalidStatusOption = worm::cli::parse({"migrate", "status", "--apply"});
        worm::cli::validate(invalidStatusOption);
      }) ||
      !rejectsArguments([] {
        const auto missingName = worm::cli::parse({"migrate", "create"});
        worm::cli::validate(missingName);
      }) ||
      !rejectsArguments([] {
        const auto invalidName = worm::cli::parse({"migrate", "create", "--name", "Create Users"});
        worm::cli::validate(invalidName);
      }) ||
      !rejectsArguments([] {
        const auto misplacedName = worm::cli::parse({"migrate", "status", "--name", "status"});
        worm::cli::validate(misplacedName);
      }) ||
      !rejectsArguments([] {
        const auto misplacedDirectory = worm::cli::parse({"check", "--directory", "migrations"});
        worm::cli::validate(misplacedDirectory);
      })) {
    std::cerr << "Migrate accepted a missing argument or an option outside its subcommand contract.\n";
    return 1;
  }

  return 0;
}
