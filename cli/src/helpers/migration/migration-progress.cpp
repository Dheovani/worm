#include "migration-progress.hpp"

#include <optional>
#include <string_view>

#include <runner.hpp>

namespace worm::cli::migration
{
  namespace
  {
    [[nodiscard]]
    std::string_view riskName(core::MigrationRisk risk) noexcept
    {
      switch (risk) {
      case core::MigrationRisk::Safe:
        return "safe";
      case core::MigrationRisk::Ambiguous:
        return "ambiguous";
      case core::MigrationRisk::Destructive:
        return "destructive";
      }
      return "unknown";
    }
  } // namespace

  MigrationProgressLogger::MigrationProgressLogger(std::ostream* output, bool json) noexcept
    : output_(output),
      json_(json)
  {}

  void MigrationProgressLogger::queryExecuted(
    std::string_view operation,
    const core::Statement& statement,
    const core::ResultSet& result,
    std::optional<core::MigrationRisk> risk) noexcept
  {
    ++queriesExecuted_;
    affectedRows_ += result.affectedRows();
    if (risk.has_value()) {
      ++statementsExecuted_;
    }
    if (output_ == nullptr) {
      return;
    }

    try {
      if (json_) {
        *output_ << "{\"event\":\"migration-query-completed\",\"operation\":";
        ExecutionMetrics::writeJsonString(*output_, operation);
        *output_ << ",\"risk\":";
        if (risk.has_value()) {
          ExecutionMetrics::writeJsonString(*output_, riskName(*risk));
        } else {
          *output_ << "null";
        }
        *output_ << ",\"sql\":";
        ExecutionMetrics::writeJsonString(*output_, statement.sql);
        *output_ << ",\"parameterCount\":" << statement.parameters.size()
                 << ",\"affectedRows\":" << result.affectedRows() << ",\"returnedRows\":" << result.rowCount()
                 << "}\n";
      } else {
        *output_ << "[migrate] Query completed: " << operation << '\n';
        if (risk.has_value()) {
          *output_ << "  Risk: " << riskName(*risk) << '\n';
        }
        *output_ << "  SQL: " << statement.sql << '\n'
                 << "  Parameters: " << statement.parameters.size() << '\n'
                 << "  Affected rows: " << result.affectedRows() << '\n'
                 << "  Returned rows: " << result.rowCount() << '\n';
      }
      output_->flush();
    } catch (...) {
      output_ = nullptr;
    }
  }

  void MigrationProgressLogger::migrationStarted(const core::MigrationArtifact& artifact) noexcept
  {
    if (output_ == nullptr) {
      return;
    }

    try {
      if (json_) {
        *output_ << "{\"event\":\"migration-started\",\"id\":";
        ExecutionMetrics::writeJsonString(*output_, artifact.id());
        *output_ << ",\"name\":";
        ExecutionMetrics::writeJsonString(*output_, artifact.name());
        *output_ << ",\"statements\":" << artifact.forward().size() << "}\n";
      } else {
        *output_ << "[migrate] Applying " << artifact.id() << " (" << artifact.name() << ", "
                 << artifact.forward().size() << " statement(s)).\n";
      }
      output_->flush();
    } catch (...) {
      output_ = nullptr;
    }
  }

  void MigrationProgressLogger::migrationCompleted(const core::MigrationArtifact& artifact) noexcept
  {
    if (output_ == nullptr) {
      return;
    }

    try {
      if (json_) {
        *output_ << "{\"event\":\"migration-applied\",\"id\":";
        ExecutionMetrics::writeJsonString(*output_, artifact.id());
        *output_ << ",\"name\":";
        ExecutionMetrics::writeJsonString(*output_, artifact.name());
        *output_ << "}\n";
      } else {
        *output_ << "[migrate] Applied " << artifact.id() << " (" << artifact.name() << ").\n";
      }
      output_->flush();
    } catch (...) {
      output_ = nullptr;
    }
  }

  void MigrationProgressLogger::migrationFailed(
    const core::MigrationArtifact& artifact,
    std::string_view reason) noexcept
  {
    if (output_ == nullptr) {
      return;
    }

    try {
      if (json_) {
        *output_ << "{\"event\":\"migration-failed\",\"id\":";
        ExecutionMetrics::writeJsonString(*output_, artifact.id());
        *output_ << ",\"name\":";
        ExecutionMetrics::writeJsonString(*output_, artifact.name());
        *output_ << ",\"reason\":";
        ExecutionMetrics::writeJsonString(*output_, reason);
        *output_ << "}\n";
      } else {
        *output_ << "[migrate] Failed " << artifact.id() << " (" << artifact.name() << "): " << reason << '\n';
      }
      output_->flush();
    } catch (...) {
      output_ = nullptr;
    }
  }

  std::size_t MigrationProgressLogger::queriesExecuted() const noexcept
  {
    return queriesExecuted_;
  }

  std::size_t MigrationProgressLogger::statementsExecuted() const noexcept
  {
    return statementsExecuted_;
  }

  std::uint64_t MigrationProgressLogger::affectedRows() const noexcept
  {
    return affectedRows_;
  }
} // namespace worm::cli::migration
