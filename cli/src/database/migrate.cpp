#include "migrate.hpp"

#include <filesystem>
#include <memory>
#include <string>

#include <core/model/migration-artifact.hpp>

#include <errors/invalid-cli-argument-exception.hpp>

namespace worm::cli::database
{
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

  ExecutionReport migrate(const Invocation& invocation)
  {
    if (invocation.migrationAction != MigrationAction::Validate) {
      throw InvalidCliArgumentException("Unsupported migration subcommand.");
    }

    const std::filesystem::path directory = invocation.arguments.directory.value_or("migrations");
    return validateMigrations(migration::discoverMigrationArtifacts(directory));
  }
} // namespace worm::cli::database
