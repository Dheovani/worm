#include <core/model/migration-history.hpp>

#include <type_traits>
#include <utility>

namespace worm::core
{
  namespace
  {
    [[nodiscard]]
    constexpr Hash combineHash(Hash seed, Hash value) noexcept
    {
      constexpr Hash hashConstant = 0x9e3779b97f4a7c15ULL;
      return seed ^ (value + hashConstant + (seed << 6U) + (seed >> 2U));
    }

    template <typename Enum>
    [[nodiscard]]
    constexpr Hash enumHash(Enum value) noexcept
    {
      return static_cast<Hash>(static_cast<std::underlying_type_t<Enum>>(value));
    }

    [[nodiscard]]
    Hash stepChecksum(const MigrationStep& step) noexcept
    {
      Hash value = enumHash(step.kind);
      value = combineHash(value, enumHash(step.risk));
      value = combineHash(value, enumHash(step.difference.kind));
      value = combineHash(value, hashCode(step.difference.table.schema().name()));
      value = combineHash(value, hashCode(step.difference.table.name()));
      value = combineHash(value, hashCode(step.difference.column));
      value = combineHash(value, hashCode(step.difference.expected));
      value = combineHash(value, hashCode(step.difference.actual));
      value = combineHash(value, hashCode(step.description));

      if (step.statement.has_value()) {
        value = combineHash(value, StatementHash{}(step.statement.value()));
      }

      return value;
    }
  } // namespace

  Hash migrationChecksum(const MigrationPlan& plan) noexcept
  {
    Hash value = 0;

    for (const MigrationStep& step : plan.steps()) {
      value = combineHash(value, stepChecksum(step));
    }

    return value;
  }

  const std::vector<MigrationRecord>& MigrationHistory::records() const noexcept
  {
    return records_;
  }

  bool MigrationHistory::empty() const noexcept
  {
    return records_.empty();
  }

  const MigrationRecord* MigrationHistory::find(std::string_view id) const noexcept
  {
    for (const MigrationRecord& record : records_) {
      if (record.id == id) {
        return &record;
      }
    }

    return nullptr;
  }

  MigrationRecord* MigrationHistory::find(std::string_view id) noexcept
  {
    for (MigrationRecord& record : records_) {
      if (record.id == id) {
        return &record;
      }
    }

    return nullptr;
  }

  bool MigrationHistory::addPending(std::string id, const MigrationPlan& plan)
  {
    if (id.empty() || find(id) != nullptr) {
      return false;
    }

    records_.push_back(
      {
        .id = std::move(id),
        .checksum = migrationChecksum(plan),
      });

    return true;
  }

  bool MigrationHistory::matchesChecksum(std::string_view id, const MigrationPlan& plan) const noexcept
  {
    const MigrationRecord* record = find(id);
    return record != nullptr && record->checksum == migrationChecksum(plan);
  }

  bool MigrationHistory::markApplied(std::string_view id, std::chrono::system_clock::time_point appliedAt)
  {
    MigrationRecord* record = find(id);
    if (record == nullptr) {
      return false;
    }

    record->state = MigrationState::Applied;
    record->appliedAt = appliedAt;
    record->rolledBackAt = std::nullopt;
    record->failureReason.clear();
    return true;
  }

  bool MigrationHistory::markRolledBack(std::string_view id, std::chrono::system_clock::time_point rolledBackAt)
  {
    MigrationRecord* record = find(id);
    if (record == nullptr) {
      return false;
    }

    record->state = MigrationState::RolledBack;
    record->rolledBackAt = rolledBackAt;
    return true;
  }

  bool MigrationHistory::markFailed(std::string_view id, std::string reason)
  {
    MigrationRecord* record = find(id);
    if (record == nullptr) {
      return false;
    }

    record->state = MigrationState::Failed;
    record->failureReason = std::move(reason);
    return true;
  }
} // namespace worm::core
