#pragma once

#include <cstddef>
#include <ostream>

#include <helpers/migration/migration-catalog.hpp>
#include <parser.hpp>
#include <runner.hpp>

namespace worm::cli::database
{
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

  [[nodiscard]]
  ExecutionReport validateMigrations(const migration::MigrationCatalog& catalog);

  [[nodiscard]]
  ExecutionReport migrate(const Invocation& invocation);
} // namespace worm::cli::database
