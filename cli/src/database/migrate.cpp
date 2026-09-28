#include "migrate.hpp"

#include <core/model/migration.hpp>
#include <core/model/schema-diff.hpp>
#include <core/persistence/migration-history-repository.hpp>
#include <core/query/migration-ddl.hpp>
#include <utils/dependency-injection.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <core/model/migration-artifact.hpp>
#include <errors/invalid-cli-argument-exception.hpp>
#include <errors/migration-exception.hpp>
#include <helpers/connection.hpp>
#include <helpers/manifest.hpp>
#include <helpers/migration/migration-file.hpp>

namespace worm::cli::database
{
  namespace
  {
    struct MigrationRuntime
    {
      explicit MigrationRuntime(const Invocation& invocation)
        : type(databaseType(invocation)),
          client(DependencyInjector<connection::Client>::get(connectionConfig(invocation, type), type)),
          sqlBuilder(DependencyInjector<core::SqlBuilder>::get(type)),
          queryBuilder(sqlBuilder),
          schema(defaultSchema(type)),
          repository(client, queryBuilder, schema),
          databaseSchema(connection::SchemaInspector{*client}.inspect())
      {}

      const connection::DatabaseType type;
      const std::shared_ptr<connection::Client> client;
      const core::SqlBuilder& sqlBuilder;
      const core::QueryBuilder queryBuilder;
      const std::string schema;
      const core::Repository<core::MigrationHistory> repository;
      const core::SchemaSnapshot databaseSchema;
    };

    [[nodiscard]]
    migration::MigrationCatalog migrationCatalog(const Invocation& invocation)
    {
      return migration::discoverMigrationArtifacts(invocation.arguments.directory.value_or("migrations"));
    }

    [[nodiscard]]
    std::string currentMigrationId()
    {
      const auto timestamp = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
      const std::chrono::sys_days day = std::chrono::floor<std::chrono::days>(timestamp);
      const std::chrono::year_month_day date{day};
      const std::chrono::hh_mm_ss time{timestamp - day};

      std::ostringstream id;
      id << std::setfill('0') << std::setw(4) << static_cast<int>(date.year()) << std::setw(2)
         << static_cast<unsigned int>(date.month()) << std::setw(2) << static_cast<unsigned int>(date.day())
         << std::setw(2) << time.hours().count() << std::setw(2) << time.minutes().count() << std::setw(2)
         << time.seconds().count();
      return id.str();
    }

    [[nodiscard]]
    core::SchemaSnapshot applicationSchema(const MigrationRuntime& runtime)
    {
      core::SchemaSnapshot schema = runtime.databaseSchema;
      std::erase_if(schema.tables, [&](const core::SchemaTableSnapshot& table) {
        return table.name == core::Repository<core::MigrationHistory>::tableName() &&
               (runtime.schema.empty() || table.schema == runtime.schema);
      });
      return schema;
    }

    void validateNewMigrationId(const std::filesystem::path& directory, std::string_view id)
    {
      std::error_code error;
      if (!std::filesystem::exists(directory, error)) {
        if (error) {
          throw MigrationException(
            "Unable to inspect migration directory '{}': {}.",
            directory.string(),
            error.message());
        }
        return;
      }

      const migration::MigrationCatalog catalog = migration::discoverMigrationArtifacts(directory);
      if (!catalog.empty() && catalog.migrations().back().artifact.id() >= id) {
        throw MigrationException(
          "Generated migration id '{}' must be greater than the latest local migration id '{}'.",
          id,
          catalog.migrations().back().artifact.id());
      }
    }

    [[nodiscard]]
    ExecutionReport createMigration(const Invocation& invocation)
    {
      if (!invocation.global.manifest.has_value()) {
        throw InvalidCliArgumentException("The 'migrate create' command requires a schema manifest.");
      }

      if (!invocation.arguments.name.has_value()) {
        throw InvalidCliArgumentException("The 'migrate create' command requires option '--name'.");
      }

      const MigrationRuntime runtime{invocation};
      const std::string manifestSchema =
        runtime.type == connection::DatabaseType::MySQL ? invocation.global.database.value_or("") : runtime.schema;
      const SchemaManifest manifest = loadManifest(*invocation.global.manifest, manifestSchema);
      const core::SchemaMetadata expected = schemaMetadata(manifest);
      const core::SchemaSnapshot actual = applicationSchema(runtime);
      const std::vector<core::SchemaDifference> differences = core::compareSchemas(expected, actual);
      auto metrics = std::make_shared<MigrationCreateMetrics>();
      metrics->differences = differences.size();
      if (differences.empty()) {
        return {
          .info = "No schema differences were found. No migration artifact was created.",
          .status = ExecutionStatus::Success,
          .metrics = std::move(metrics),
        };
      }

      const core::MigrationDdlPlan ddl = core::compileMigrationDdl(
        core::generateMigrationPlan(differences),
        expected,
        actual,
        runtime.sqlBuilder);
      std::vector<core::MigrationStatement> statements;
      for (const core::MigrationDdlStep& step : ddl.steps()) {
        for (const core::Statement& statement : step.statements) {
          statements.push_back(
            {
              .description = step.description,
              .sql = statement.sql,
              .risk = step.risk,
            });
          ++metrics->statements;
          switch (step.risk) {
          case core::MigrationRisk::Safe:
            ++metrics->safeStatements;
            break;
          case core::MigrationRisk::Ambiguous:
            ++metrics->ambiguousStatements;
            break;
          case core::MigrationRisk::Destructive:
            ++metrics->destructiveStatements;
            break;
          }
        }
      }

      const std::string id = currentMigrationId();
      const std::filesystem::path directory = invocation.arguments.directory.value_or("migrations");
      validateNewMigrationId(directory, id);
      const core::MigrationArtifact artifact = core::makeMigrationArtifact(
        id,
        *invocation.arguments.name,
        std::string{runtime.sqlBuilder.databaseName()},
        std::move(statements));
      const std::filesystem::path path = directory / (artifact.id() + "_" + artifact.name() + ".worm.json");
      migration::saveMigrationArtifact(path, artifact);
      metrics->generatedArtifacts = 1;

      return {
        .info = "Created migration artifact '" + path.generic_string() + "'. Review it before execution.",
        .status = ExecutionStatus::Success,
        .metrics = std::move(metrics),
      };
    }

    [[nodiscard]]
    bool hasMigrationHistoryTable(const core::SchemaSnapshot& databaseSchema, const std::string& schema)
    {
      if (!schema.empty()) {
        return databaseSchema.findTable(schema, core::Repository<core::MigrationHistory>::tableName()) != nullptr;
      }

      return std::ranges::any_of(
        databaseSchema.tables,
        [](const core::SchemaTableSnapshot& table) {
          return table.name == core::Repository<core::MigrationHistory>::tableName();
        });
    }

    [[nodiscard]]
    ExecutionReport migrationStatusReport(const Invocation& invocation)
    {
      const migration::MigrationCatalog catalog = migrationCatalog(invocation);
      const MigrationRuntime runtime{invocation};

      if (!hasMigrationHistoryTable(runtime.databaseSchema, runtime.schema)) {
        return migrationStatus(catalog, core::MigrationHistory{});
      }

      runtime.repository.initialize(runtime.databaseSchema);
      return migrationStatus(catalog, runtime.repository.load());
    }

    [[nodiscard]]
    ExecutionReport rollbackMigration(const Invocation& invocation)
    {
      const migration::MigrationCatalog catalog = migrationCatalog(invocation);
      const MigrationRuntime runtime{invocation};
      if (!hasMigrationHistoryTable(runtime.databaseSchema, runtime.schema)) {
        throw MigrationException("Cannot roll back migrations because migration history does not exist.");
      }

      runtime.repository.initialize(runtime.databaseSchema);
      const core::MigrationHistory history = runtime.repository.load();
      const core::MigrationRecord* record = history.latestApplied();
      auto metrics = std::make_shared<MigrationRollbackMetrics>();
      if (record == nullptr) {
        return {
          .info = "There are no applied migrations to roll back.",
          .status = ExecutionStatus::Success,
          .metrics = std::move(metrics),
        };
      }

      const migration::MigrationFile* file = catalog.find(record->id);
      if (file == nullptr) {
        throw MigrationException("Applied migration '{}' is missing from the local migration directory.", record->id);
      }

      if (file->artifact.checksum() != record->checksum) {
        throw MigrationException(
          "Applied migration '{}' has checksum '{}', but the local artifact has checksum '{}'.",
          record->id,
          record->checksum,
          file->artifact.checksum());
      }

      const core::MigrationExecutionPlan plan = core::compileMigrationExecutionPlan(
        file->artifact,
        runtime.sqlBuilder,
        core::MigrationDirection::Rollback);

      runtime.repository.rollback(plan, core::MigrationConfirmation::Destructive);
      metrics->migrations = 1;
      metrics->rolledBackMigrations = 1;

      return {
        .info = "Migration rolled back successfully.",
        .status = ExecutionStatus::Success,
        .metrics = std::move(metrics),
      };
    }
  } // namespace

  void MigrationCreateMetrics::writeText(std::ostream& out) const
  {
    printMetric(out, "Differences", differences);
    printMetric(out, "Statements", statements);
    printMetric(out, "Safe statements", safeStatements);
    printMetric(out, "Ambiguous statements", ambiguousStatements);
    printMetric(out, "Destructive statements", destructiveStatements);
    printMetric(out, "Generated artifacts", generatedArtifacts);
  }

  void MigrationCreateMetrics::writeJson(std::ostream& out) const
  {
    out << "{\"differences\":" << differences << ",\"statements\":" << statements
        << ",\"safeStatements\":" << safeStatements << ",\"ambiguousStatements\":" << ambiguousStatements
        << ",\"destructiveStatements\":" << destructiveStatements
        << ",\"generatedArtifacts\":" << generatedArtifacts << '}';
  }

  void MigrationValidateMetrics::writeText(std::ostream& out) const
  {
    printMetric(out, "Migrations", migrations);
    printMetric(out, "Statements", statements);
    printMetric(out, "Safe statements", safeStatements);
    printMetric(out, "Ambiguous statements", ambiguousStatements);
    printMetric(out, "Destructive statements", destructiveStatements);
    printMetric(out, "Reversible migrations", reversibleMigrations);
  }

  void MigrationValidateMetrics::writeJson(std::ostream& out) const
  {
    out << "{\"migrations\":" << migrations << ",\"statements\":" << statements
        << ",\"safeStatements\":" << safeStatements << ",\"ambiguousStatements\":" << ambiguousStatements
        << ",\"destructiveStatements\":" << destructiveStatements
        << ",\"reversibleMigrations\":" << reversibleMigrations << '}';
  }

  void MigrationStatusMetrics::writeText(std::ostream& out) const
  {
    printMetric(out, "Migrations", migrations);
    printMetric(out, "Applied migrations", appliedMigrations);
    printMetric(out, "Pending migrations", pendingMigrations);
    printMetric(out, "Failed migrations", failedMigrations);
    printMetric(out, "Missing migrations", missingMigrations);
    printMetric(out, "Checksum divergences", checksumDivergentMigrations);
  }

  void MigrationStatusMetrics::writeJson(std::ostream& out) const
  {
    out << "{\"migrations\":" << migrations << ",\"appliedMigrations\":" << appliedMigrations
        << ",\"pendingMigrations\":" << pendingMigrations << ",\"failedMigrations\":" << failedMigrations
        << ",\"missingMigrations\":" << missingMigrations
        << ",\"checksumDivergentMigrations\":" << checksumDivergentMigrations << '}';
  }

  void MigrationRollbackMetrics::writeText(std::ostream& out) const
  {
    printMetric(out, "Migrations", migrations);
    printMetric(out, "Rolled back migrations", rolledBackMigrations);
  }

  void MigrationRollbackMetrics::writeJson(std::ostream& out) const
  {
    out << "{\"migrations\":" << migrations << ",\"rolledBackMigrations\":" << rolledBackMigrations << '}';
  }

  ExecutionReport validateMigrations(const migration::MigrationCatalog& catalog)
  {
    migration::validateMigrationCatalog(catalog);

    auto metrics = std::make_shared<MigrationValidateMetrics>();
    metrics->migrations = catalog.migrations().size();

    for (const migration::MigrationFile& file : catalog.migrations()) {
      const core::MigrationArtifact& artifact = file.artifact;
      metrics->statements += artifact.forward().size();
      if (artifact.rollback().has_value()) {
        ++metrics->reversibleMigrations;
      }

      for (const core::MigrationStatement& statement : artifact.forward()) {
        switch (statement.risk) {
        case core::MigrationRisk::Safe:
          ++metrics->safeStatements;
          break;
        case core::MigrationRisk::Ambiguous:
          ++metrics->ambiguousStatements;
          break;
        case core::MigrationRisk::Destructive:
          ++metrics->destructiveStatements;
          break;
        }
      }
    }

    return {
      .info = "Migration catalog is valid.",
      .status = ExecutionStatus::Success,
      .metrics = std::move(metrics),
    };
  }

  ExecutionReport migrationStatus(const migration::MigrationCatalog& catalog, const core::MigrationHistory& history)
  {
    migration::validateMigrationCatalog(catalog);

    auto metrics = std::make_shared<MigrationStatusMetrics>();
    for (const migration::MigrationFile& file : catalog.migrations()) {
      const core::MigrationArtifact& artifact = file.artifact;
      const core::MigrationRecord* record = history.find(artifact.id());
      if (record == nullptr) {
        ++metrics->pendingMigrations;
        continue;
      }
      if (record->checksum != artifact.checksum()) {
        ++metrics->checksumDivergentMigrations;
        continue;
      }

      switch (record->state) {
      case core::MigrationState::Applied:
        ++metrics->appliedMigrations;
        break;
      case core::MigrationState::Pending:
      case core::MigrationState::RolledBack:
        ++metrics->pendingMigrations;
        break;
      case core::MigrationState::Failed:
        ++metrics->failedMigrations;
        break;
      }
    }

    for (const core::MigrationRecord& record : history.records()) {
      if (catalog.find(record.id) == nullptr) {
        ++metrics->missingMigrations;
      }
    }
    metrics->migrations = catalog.migrations().size() + metrics->missingMigrations;

    std::string info = "Migration status retrieved successfully.";
    ExecutionStatus status = ExecutionStatus::Success;
    if (metrics->missingMigrations != 0 || metrics->checksumDivergentMigrations != 0) {
      info = "Migration drift detected.";
      status = ExecutionStatus::DriftDetected;
    } else if (metrics->failedMigrations != 0) {
      info = "Some migrations have failed.";
      status = ExecutionStatus::IssuesDetected;
    }

    return {
      .info = info,
      .status = status,
      .metrics = std::move(metrics),
    };
  }

  ExecutionReport migrate(const Invocation& invocation)
  {
    if (!invocation.migrationAction.has_value()) {
      throw InvalidCliArgumentException("The 'migrate' command requires a subcommand.");
    }

    switch (*invocation.migrationAction) {
    case MigrationAction::Create:
      return createMigration(invocation);
    case MigrationAction::Validate:
      return validateMigrations(migrationCatalog(invocation));
    case MigrationAction::Status:
      return migrationStatusReport(invocation);
    case MigrationAction::Rollback:
      return rollbackMigration(invocation);
    }

    throw InvalidCliArgumentException("Unsupported migration subcommand.");
  }
} // namespace worm::cli::database
