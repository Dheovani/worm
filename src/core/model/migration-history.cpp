#include <core/model/migration-history.hpp>

#include <errors/migration-exception.hpp>

#include <unordered_set>
#include <utility>

namespace worm::core
{
  MigrationHistory::MigrationHistory(std::vector<MigrationRecord> records)
    : records_(std::move(records))
  {
    std::unordered_set<std::string_view> ids;
    ids.reserve(records_.size());
    for (const MigrationRecord& record : records_) {
      if (record.id.empty() || record.name.empty() || !isMigrationArtifactChecksum(record.checksum)) {
        throw MigrationException("Migration history records require a non-empty id, name, and SHA-256 checksum.");
      }
      if (!ids.insert(record.id).second) {
        throw MigrationException("Migration history contains duplicate id '{}'.", record.id);
      }
    }
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

  bool MigrationHistory::addPending(const MigrationArtifact& artifact)
  {
    validateMigrationArtifact(artifact);
    if (find(artifact.id()) != nullptr) {
      return false;
    }

    records_.push_back(
      {
        .id = artifact.id(),
        .name = artifact.name(),
        .checksum = artifact.checksum(),
      });

    return true;
  }

  bool MigrationHistory::matchesChecksum(std::string_view id, const MigrationArtifact& artifact) const noexcept
  {
    const MigrationRecord* record = find(id);
    return record != nullptr && record->checksum == artifact.checksum();
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
