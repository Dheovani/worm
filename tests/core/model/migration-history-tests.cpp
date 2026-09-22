#include <core/model/migration-history.hpp>

#include <errors/migration-exception.hpp>

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

namespace
{
  worm::core::MigrationArtifact artifactForColumn(std::string id, std::string column)
  {
    return worm::core::makeMigrationArtifact(
      std::move(id),
      "add-" + column,
      "sqlite",
      {
        {
          .description = "Add " + column,
          .sql = "alter table users add column " + column + " text",
        },
      });
  }
} // namespace

int main()
{
  const worm::core::MigrationArtifact nameArtifact = artifactForColumn("20260820000100", "name");
  const worm::core::MigrationArtifact emailArtifact = artifactForColumn("20260820000200", "email");

  if (nameArtifact.checksum() == emailArtifact.checksum()) {
    std::cerr << "Migration artifacts unexpectedly produced the same checksum.\n";
    return 1;
  }

  worm::core::MigrationHistory history;
  if (!history.empty() || !history.addPending(nameArtifact) || history.addPending(nameArtifact)) {
    std::cerr << "Migration history did not accept only unique migration artifacts.\n";
    return 1;
  }

  if (!history.matchesChecksum(nameArtifact.id(), nameArtifact) ||
      history.matchesChecksum(nameArtifact.id(), emailArtifact) || history.matchesChecksum("missing", nameArtifact)) {
    std::cerr << "Migration history did not validate artifact checksums correctly.\n";
    return 1;
  }

  const worm::core::MigrationRecord* pending = history.find(nameArtifact.id());
  if (pending == nullptr || pending->name != nameArtifact.name() || pending->checksum != nameArtifact.checksum() ||
      pending->state != worm::core::MigrationState::Pending) {
    std::cerr << "Migration history did not preserve pending artifact metadata.\n";
    return 1;
  }

  const auto appliedAt = std::chrono::system_clock::time_point{std::chrono::seconds{10}};
  if (!history.markApplied(nameArtifact.id(), appliedAt)) {
    std::cerr << "Migration history did not mark a migration as applied.\n";
    return 1;
  }

  const worm::core::MigrationRecord* applied = history.find(nameArtifact.id());
  if (applied == nullptr || applied->state != worm::core::MigrationState::Applied || applied->appliedAt != appliedAt ||
      applied->rolledBackAt.has_value() || !applied->failureReason.empty()) {
    std::cerr << "Migration history did not preserve application metadata.\n";
    return 1;
  }

  if (!history.markFailed(nameArtifact.id(), "constraint failed")) {
    std::cerr << "Migration history did not mark a migration as failed.\n";
    return 1;
  }

  const worm::core::MigrationRecord* failed = history.find(nameArtifact.id());
  if (failed == nullptr || failed->state != worm::core::MigrationState::Failed ||
      failed->failureReason != "constraint failed") {
    std::cerr << "Migration history did not preserve failure metadata.\n";
    return 1;
  }

  const auto rolledBackAt = std::chrono::system_clock::time_point{std::chrono::seconds{20}};
  if (!history.markRolledBack(nameArtifact.id(), rolledBackAt)) {
    std::cerr << "Migration history did not mark a migration as rolled back.\n";
    return 1;
  }

  const worm::core::MigrationRecord* rolledBack = history.find(nameArtifact.id());
  if (rolledBack == nullptr || rolledBack->state != worm::core::MigrationState::RolledBack ||
      rolledBack->appliedAt != appliedAt || rolledBack->rolledBackAt != rolledBackAt) {
    std::cerr << "Migration history did not preserve rollback metadata.\n";
    return 1;
  }

  if (history.markApplied("missing", appliedAt) || history.markRolledBack("missing", rolledBackAt) ||
      history.markFailed("missing", "missing")) {
    std::cerr << "Migration history accepted state changes for a missing migration.\n";
    return 1;
  }

  bool duplicateHistoryRejected = false;
  try {
    const worm::core::MigrationRecord record{
      .id = nameArtifact.id(),
      .name = nameArtifact.name(),
      .checksum = nameArtifact.checksum(),
    };
    static_cast<void>(worm::core::MigrationHistory{{record, record}});
  } catch (const worm::MigrationException&) {
    duplicateHistoryRejected = true;
  }

  if (!duplicateHistoryRejected) {
    std::cerr << "Migration history accepted duplicate persisted records.\n";
    return 1;
  }

  bool invalidChecksumRejected = false;
  try {
    static_cast<void>(worm::core::MigrationHistory{{
      {
        .id = nameArtifact.id(),
        .name = nameArtifact.name(),
        .checksum = "not-a-checksum",
      },
    }});
  } catch (const worm::MigrationException&) {
    invalidChecksumRejected = true;
  }
  if (!invalidChecksumRejected) {
    std::cerr << "Migration history accepted an invalid persisted checksum.\n";
    return 1;
  }

  return 0;
}
