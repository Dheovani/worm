#pragma once

#include <core/model/migration-artifact.hpp>
#include <core/persistence/migration-history-repository.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <ostream>
#include <string_view>

namespace worm::cli::migration
{
  class MigrationProgressLogger final : public core::MigrationQueryObserver
  {
  public:
    MigrationProgressLogger(std::ostream* output, bool json) noexcept;

    void queryExecuted(
      std::string_view operation,
      const core::Statement& statement,
      const core::ResultSet& result,
      std::optional<core::MigrationRisk> risk) noexcept override;

    void migrationStarted(const core::MigrationArtifact& artifact) noexcept;
    void migrationCompleted(const core::MigrationArtifact& artifact) noexcept;
    void migrationFailed(const core::MigrationArtifact& artifact, std::string_view reason) noexcept;

    [[nodiscard]]
    std::size_t queriesExecuted() const noexcept;

    [[nodiscard]]
    std::size_t statementsExecuted() const noexcept;

    [[nodiscard]]
    std::uint64_t affectedRows() const noexcept;

  private:
    std::ostream* output_;
    bool json_;
    std::size_t queriesExecuted_{};
    std::size_t statementsExecuted_{};
    std::uint64_t affectedRows_{};
  };
} // namespace worm::cli::migration
