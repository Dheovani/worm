#include "migrate.hpp"

#include <core/persistence/migration-history-repository.hpp>
#include <utils/dependency-injection.hpp>

#include <algorithm>
#include <memory>
#include <string>

#include <core/model/migration-artifact.hpp>
#include <errors/invalid-cli-argument-exception.hpp>
#include <errors/migration-exception.hpp>
#include <helpers/connection.hpp>

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
