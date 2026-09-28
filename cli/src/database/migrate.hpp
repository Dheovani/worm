#pragma once

#include <cstddef>
#include <ostream>

#include <core/model/migration-history.hpp>
#include <helpers/migration/migration-catalog.hpp>
#include <parser.hpp>
#include <runner.hpp>

namespace worm::cli::database
{
  struct MigrationCreateMetrics final : ExecutionMetrics
  {
    std::size_t differences{};
    std::size_t statements{};
    std::size_t safeStatements{};
    std::size_t ambiguousStatements{};
    std::size_t destructiveStatements{};
    std::size_t generatedArtifacts{};

    void writeText(std::ostream& out) const override;
    void writeJson(std::ostream& out) const override;
  };

  struct MigrationValidateMetrics final : ExecutionMetrics
  {
    std::size_t migrations{};
    std::size_t statements{};
    std::size_t safeStatements{};
    std::size_t ambiguousStatements{};
    std::size_t destructiveStatements{};
    std::size_t reversibleMigrations{};

    void writeText(std::ostream& out) const override;
    void writeJson(std::ostream& out) const override;
  };

  struct MigrationStatusMetrics final : ExecutionMetrics
  {
    std::size_t migrations{};
    std::size_t appliedMigrations{};
    std::size_t pendingMigrations{};
    std::size_t failedMigrations{};
    std::size_t missingMigrations{};
    std::size_t checksumDivergentMigrations{};

    void writeText(std::ostream& out) const override;
    void writeJson(std::ostream& out) const override;
  };

  struct MigrationRollbackMetrics final : ExecutionMetrics
  {
    std::size_t migrations{};
    std::size_t rolledBackMigrations{};

    void writeText(std::ostream& out) const override;
    void writeJson(std::ostream& out) const override;
  };

  [[nodiscard]]
  ExecutionReport validateMigrations(const migration::MigrationCatalog& catalog);

  [[nodiscard]]
  ExecutionReport migrationStatus(const migration::MigrationCatalog& catalog, const core::MigrationHistory& history);

  [[nodiscard]]
  ExecutionReport migrate(const Invocation& invocation);
} // namespace worm::cli::database
