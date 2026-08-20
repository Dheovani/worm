#pragma once

#include <core/model/migration.hpp>
#include <utils/hash.hpp>

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace worm::core
{
  enum class MigrationState
  {
    Pending,
    Applied,
    RolledBack,
    Failed
  };

  struct MigrationRecord
  {
    std::string id;
    Hash checksum{};
    MigrationState state{MigrationState::Pending};
    std::optional<std::chrono::system_clock::time_point> appliedAt = std::nullopt;
    std::optional<std::chrono::system_clock::time_point> rolledBackAt = std::nullopt;
    std::string failureReason;
  };

  [[nodiscard]]
  Hash migrationChecksum(const MigrationPlan& plan) noexcept;

  class MigrationHistory
  {
  public:
    [[nodiscard]]
    const std::vector<MigrationRecord>& records() const noexcept;

    [[nodiscard]]
    bool empty() const noexcept;

    [[nodiscard]]
    const MigrationRecord* find(std::string_view id) const noexcept;

    [[nodiscard]]
    MigrationRecord* find(std::string_view id) noexcept;

    [[nodiscard]]
    bool addPending(std::string id, const MigrationPlan& plan);

    [[nodiscard]]
    bool matchesChecksum(std::string_view id, const MigrationPlan& plan) const noexcept;

    [[nodiscard]]
    bool markApplied(std::string_view id, std::chrono::system_clock::time_point appliedAt);

    [[nodiscard]]
    bool markRolledBack(std::string_view id, std::chrono::system_clock::time_point rolledBackAt);

    [[nodiscard]]
    bool markFailed(std::string_view id, std::string reason);

  private:
    std::vector<MigrationRecord> records_;
  };
} // namespace worm::core
