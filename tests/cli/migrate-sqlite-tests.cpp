#include <database/migrate.hpp>
#include <helpers/file.hpp>
#include <helpers/manifest.hpp>
#include <helpers/migration/migration-catalog.hpp>
#include <helpers/migration/migration-file.hpp>
#include <parser.hpp>
#include <validator.hpp>

#include <connection/schema-inspector.hpp>
#include <core/model/migration-artifact.hpp>
#include <core/persistence/migration-history-repository.hpp>
#include <errors/migration-exception.hpp>
#include <utils/dependency-injection.hpp>

#include <filesystem>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
  class TemporaryDirectory
  {
  public:
    TemporaryDirectory()
      : path_(std::filesystem::temp_directory_path() / "worm-cli-migrate-sqlite-tests")
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

  [[nodiscard]]
  worm::core::MigrationArtifact migrationArtifact(std::string id, std::string name, std::string table)
  {
    return worm::core::makeMigrationArtifact(
      std::move(id),
      std::move(name),
      "sqlite",
      {
        {
          .description = "Create table",
          .sql = "create table " + table + " (id integer primary key)",
        },
      },
      std::vector<worm::core::MigrationStatement>{
        {
          .description = "Drop table",
          .sql = "drop table " + table,
          .risk = worm::core::MigrationRisk::Destructive,
        },
      });
  }
} // namespace

int main()
{
  const TemporaryDirectory temporary;
  const std::filesystem::path migrations = temporary.path() / "migrations";
  const std::filesystem::path database = temporary.path() / "worm.sqlite";
  std::filesystem::create_directories(migrations);

  const worm::core::MigrationArtifact first = migrationArtifact("20260928000100", "create-first", "rollback_first");
  const worm::core::MigrationArtifact second = migrationArtifact("20260928000200", "create-second", "rollback_second");
  worm::cli::migration::saveMigrationArtifact(migrations / "20260928000100_create-first.worm.json", first);
  worm::cli::migration::saveMigrationArtifact(migrations / "20260928000200_create-second.worm.json", second);

  const worm::connection::ConnectionConfig config{.dbname = database.string()};
  const auto type = worm::connection::DatabaseType::SQLite;
  const std::shared_ptr<worm::connection::Client> client =
    worm::DependencyInjector<worm::connection::Client>::get(config, type);
  const worm::core::SqlBuilder& sqlBuilder = worm::DependencyInjector<worm::core::SqlBuilder>::get(type);
  const worm::core::QueryBuilder queryBuilder{sqlBuilder};
  const worm::core::Repository<worm::core::MigrationHistory> repository{client, queryBuilder, "main"};
  const worm::connection::SchemaInspector inspector{*client};
  repository.initialize(inspector.inspect());

  for (const worm::core::MigrationArtifact* artifact : {&first, &second}) {
    repository.addPending(*artifact);
    repository.apply(worm::core::compileMigrationExecutionPlan(*artifact, sqlBuilder));
  }

  bool outOfOrderRollbackRejected = false;
  try {
    repository.rollback(
      worm::core::compileMigrationExecutionPlan(first, sqlBuilder, worm::core::MigrationDirection::Rollback),
      worm::core::MigrationConfirmation::Destructive);
  } catch (const worm::MigrationException&) {
    outOfOrderRollbackRejected = true;
  }
  if (!outOfOrderRollbackRejected) {
    std::cerr << "Migration repository accepted an out-of-order rollback.\n";
    return 1;
  }

  worm::cli::Invocation invocation = worm::cli::parse(
    {"--driver",
      "sqlite",
      "--database",
      database.string(),
      "migrate",
      "--rollback",
      "--directory",
      migrations.string()});
  worm::cli::validate(invocation);
  if (invocation.migrationAction != worm::cli::MigrationAction::Rollback) {
    std::cerr << "Migrate rollback was not parsed as a migration action.\n";
    return 1;
  }

  const worm::cli::ExecutionReport secondRollback = worm::cli::database::migrate(invocation);
  const auto secondMetrics =
    std::dynamic_pointer_cast<const worm::cli::database::MigrationRollbackMetrics>(secondRollback.metrics);
  const worm::core::MigrationHistory historyAfterSecond = repository.load();
  if (secondRollback.status != worm::cli::ExecutionStatus::Success || secondMetrics == nullptr ||
      secondMetrics->migrations != 1 || secondMetrics->rolledBackMigrations != 1 ||
      historyAfterSecond.find(first.id())->state != worm::core::MigrationState::Applied ||
      historyAfterSecond.find(second.id())->state != worm::core::MigrationState::RolledBack ||
      inspector.inspect().findTable("main", "rollback_first") == nullptr ||
      inspector.inspect().findTable("main", "rollback_second") != nullptr) {
    std::cerr << "Migrate rollback did not revert only the latest applied migration.\n";
    return 1;
  }

  std::ostringstream text;
  std::ostringstream json;
  secondMetrics->writeText(text);
  secondMetrics->writeJson(json);
  if (text.str().find("Rolled back migrations") == std::string::npos ||
      json.str().find("\"rolledBackMigrations\":1") == std::string::npos) {
    std::cerr << "Migrate rollback did not render its metrics.\n";
    return 1;
  }

  static_cast<void>(worm::cli::database::migrate(invocation));
  const worm::core::MigrationHistory historyAfterFirst = repository.load();
  if (historyAfterFirst.latestApplied() != nullptr ||
      historyAfterFirst.find(first.id())->state != worm::core::MigrationState::RolledBack ||
      inspector.inspect().findTable("main", "rollback_first") != nullptr) {
    std::cerr << "Migrate rollback did not proceed through applied migrations in reverse order.\n";
    return 1;
  }

  const worm::cli::ExecutionReport emptyRollback = worm::cli::database::migrate(invocation);
  const auto emptyMetrics =
    std::dynamic_pointer_cast<const worm::cli::database::MigrationRollbackMetrics>(emptyRollback.metrics);
  if (emptyRollback.status != worm::cli::ExecutionStatus::Success || emptyMetrics == nullptr ||
      emptyMetrics->migrations != 0 || emptyMetrics->rolledBackMigrations != 0) {
    std::cerr << "Migrate rollback did not handle an empty applied migration stack.\n";
    return 1;
  }

  worm::cli::Invocation reapplyInvocation = worm::cli::parse(
    {"--driver", "sqlite", "--database", database.string(), "migrate", "--apply", "--directory", migrations.string()});
  worm::cli::validate(reapplyInvocation);
  const worm::cli::ExecutionReport reapplyReport = worm::cli::database::migrate(reapplyInvocation);
  const auto reapplyMetrics =
    std::dynamic_pointer_cast<const worm::cli::database::MigrationApplyMetrics>(reapplyReport.metrics);
  if (reapplyReport.status != worm::cli::ExecutionStatus::Success || reapplyMetrics == nullptr ||
      reapplyMetrics->pendingMigrations != 2 || reapplyMetrics->appliedMigrations != 2 ||
      inspector.inspect().findTable("main", "rollback_first") == nullptr ||
      inspector.inspect().findTable("main", "rollback_second") == nullptr) {
    std::cerr << "Migrate apply did not reapply rolled-back migrations in order.\n";
    return 1;
  }

  const std::filesystem::path manifestPath = temporary.path() / "worm-schema.json";
  const std::filesystem::path createDirectory = temporary.path() / "created-migrations";
  const std::filesystem::path createDatabase = temporary.path() / "worm-create.sqlite";
  const worm::cli::SchemaManifest manifest{
    .entities =
      {
        {
          .name = "User",
          .table =
            {
              .schema = "main",
              .name = "users",
              .columns =
                {
                  {
                    .name = "id",
                    .type = {.kind = worm::core::ColumnTypeKind::Int64},
                    .nullable = false,
                    .generated = true,
                  },
                },
              .primaryKey = {"id"},
            },
        },
      },
  };
  worm::cli::writeGeneratedFile(manifestPath, worm::cli::serializeManifest(manifest));

  worm::cli::Invocation createInvocation = worm::cli::parse(
    {"--manifest",
      manifestPath.string(),
      "--driver",
      "sqlite",
      "--database",
      createDatabase.string(),
      "migrate",
      "--create",
      "--name",
      "create-users",
      "--directory",
      createDirectory.string()});
  worm::cli::validate(createInvocation);
  const worm::cli::ExecutionReport createReport = worm::cli::database::migrate(createInvocation);
  const auto createMetrics =
    std::dynamic_pointer_cast<const worm::cli::database::MigrationCreateMetrics>(createReport.metrics);
  const worm::cli::migration::MigrationCatalog createdCatalog =
    worm::cli::migration::discoverMigrationArtifacts(createDirectory);
  if (createReport.status != worm::cli::ExecutionStatus::Success || createMetrics == nullptr ||
      createMetrics->differences != 1 || createMetrics->statements == 0 || createMetrics->generatedArtifacts != 1 ||
      createdCatalog.migrations().size() != 1 ||
      createdCatalog.migrations().front().artifact.name() != "create-users" ||
      createdCatalog.migrations().front().artifact.database() != "sqlite" ||
      createdCatalog.migrations().front().artifact.rollback().has_value()) {
    std::cerr << "Migrate create did not generate a forward-only artifact without changing the database.\n";
    return 1;
  }

  std::ostringstream createJson;
  createMetrics->writeJson(createJson);
  if (createJson.str().find("\"generatedArtifacts\":1") == std::string::npos) {
    std::cerr << "Migrate create did not render its generation metrics.\n";
    return 1;
  }

  const worm::connection::ConnectionConfig createConfig{.dbname = createDatabase.string()};
  const std::shared_ptr<worm::connection::Client> createClient =
    worm::DependencyInjector<worm::connection::Client>::get(createConfig, type);
  const worm::connection::SchemaInspector createInspector{*createClient};
  if (createInspector.inspect().findTable("main", "users") != nullptr) {
    std::cerr << "Migrate create changed the database before migrate apply.\n";
    return 1;
  }

  worm::cli::Invocation applyInvocation = worm::cli::parse(
    {"--driver",
      "sqlite",
      "--database",
      createDatabase.string(),
      "migrate",
      "--apply",
      "--directory",
      createDirectory.string()});
  worm::cli::validate(applyInvocation);
  std::ostringstream progress;
  const worm::cli::ExecutionReport applyReport = worm::cli::database::migrate(applyInvocation, &progress);
  const auto applyMetrics =
    std::dynamic_pointer_cast<const worm::cli::database::MigrationApplyMetrics>(applyReport.metrics);
  const worm::core::Repository<worm::core::MigrationHistory> createRepository{createClient, queryBuilder, "main"};
  createRepository.initialize(createInspector.inspect());
  const worm::core::MigrationHistory createHistory = createRepository.load();
  const worm::core::MigrationArtifact& createdArtifact = createdCatalog.migrations().front().artifact;
  const worm::core::MigrationRecord* createdRecord = createHistory.find(createdArtifact.id());
  if (applyReport.status != worm::cli::ExecutionStatus::Success || applyMetrics == nullptr ||
      applyMetrics->migrations != 1 || applyMetrics->pendingMigrations != 1 || applyMetrics->appliedMigrations != 1 ||
      applyMetrics->alreadyAppliedMigrations != 0 || applyMetrics->statementsExecuted == 0 ||
      createdRecord == nullptr || createdRecord->state != worm::core::MigrationState::Applied ||
      createInspector.inspect().findTable("main", "users") == nullptr ||
      progress.str().find("[migrate] Query completed:") == std::string::npos ||
      progress.str().find("SQL: create table") == std::string::npos ||
      progress.str().find("Affected rows:") == std::string::npos ||
      progress.str().find("[migrate] Applied ") == std::string::npos) {
    std::cerr << "Migrate apply did not execute and report the pending migration in real time.\n";
    return 1;
  }

  std::ostringstream applyJson;
  applyMetrics->writeJson(applyJson);
  if (applyJson.str().find("\"appliedMigrations\":1") == std::string::npos ||
      applyJson.str().find("\"statementsExecuted\":") == std::string::npos) {
    std::cerr << "Migrate apply did not render its execution metrics.\n";
    return 1;
  }

  std::ostringstream secondProgress;
  const worm::cli::ExecutionReport secondApply = worm::cli::database::migrate(applyInvocation, &secondProgress);
  const auto secondApplyMetrics =
    std::dynamic_pointer_cast<const worm::cli::database::MigrationApplyMetrics>(secondApply.metrics);
  if (secondApply.status != worm::cli::ExecutionStatus::Success || secondApplyMetrics == nullptr ||
      secondApplyMetrics->pendingMigrations != 0 || secondApplyMetrics->appliedMigrations != 0 ||
      secondApplyMetrics->alreadyAppliedMigrations != 1) {
    std::cerr << "Migrate apply did not skip an already applied migration.\n";
    return 1;
  }

  worm::cli::Invocation jsonApplyInvocation = applyInvocation;
  jsonApplyInvocation.global.format = "json";
  std::ostringstream jsonProgress;
  static_cast<void>(worm::cli::database::migrate(jsonApplyInvocation, &jsonProgress));
  if (jsonProgress.str().find("{\"event\":\"migration-query-completed\"") == std::string::npos ||
      jsonProgress.str().find("\"parameterCount\":") == std::string::npos ||
      jsonProgress.str().find("\"affectedRows\":") == std::string::npos) {
    std::cerr << "Migrate apply did not emit structured real-time query logs.\n";
    return 1;
  }

  const worm::cli::ExecutionReport noDifferenceReport = worm::cli::database::migrate(createInvocation);
  const auto noDifferenceMetrics =
    std::dynamic_pointer_cast<const worm::cli::database::MigrationCreateMetrics>(noDifferenceReport.metrics);
  if (noDifferenceReport.status != worm::cli::ExecutionStatus::Success || noDifferenceMetrics == nullptr ||
      noDifferenceMetrics->differences != 0 || noDifferenceMetrics->generatedArtifacts != 0 ||
      worm::cli::migration::discoverMigrationArtifacts(createDirectory).migrations().size() != 1) {
    std::cerr << "Migrate create generated an artifact for a compatible schema or the Worm history table.\n";
    return 1;
  }

  return 0;
}
