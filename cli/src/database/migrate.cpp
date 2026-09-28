#include "migrate.hpp"

#include <core/persistence/migration-history-repository.hpp>
#include <utils/dependency-injection.hpp>

#include <algorithm>
#include <memory>
#include <string>

#include <core/model/migration-artifact.hpp>
#include <errors/invalid-cli-argument-exception.hpp>
#include <helpers/connection.hpp>

namespace worm::cli::database
{
  namespace
  {
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
      const connection::DatabaseType type = databaseType(invocation);
      const connection::ConnectionConfig config = connectionConfig(invocation, type);
      const std::shared_ptr<connection::Client> client = DependencyInjector<connection::Client>::get(config, type);
      const core::QueryBuilder queryBuilder = DependencyInjector<core::QueryBuilder>::get(type);
      const std::string schema = defaultSchema(type);
      const core::Repository<core::MigrationHistory> repository{client, queryBuilder, schema};
      const core::SchemaSnapshot databaseSchema = connection::SchemaInspector{*client}.inspect();

      if (!hasMigrationHistoryTable(databaseSchema, schema)) {
        return migrationStatus(catalog, core::MigrationHistory{});
      }

      repository.initialize(databaseSchema);
      return migrationStatus(catalog, repository.load());
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
    }

    throw InvalidCliArgumentException("Unsupported migration subcommand.");
  }
} // namespace worm::cli::database
