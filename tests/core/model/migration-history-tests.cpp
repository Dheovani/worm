#include <core/model/migration-history.hpp>

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

namespace
{
  worm::core::MigrationPlan planForColumn(std::string column)
  {
    return worm::core::generateMigrationPlan(
      std::vector<worm::core::SchemaDifference>{
        {
          .kind = worm::core::SchemaDifferenceKind::MissingColumn,
          .table = worm::core::Table{"users"},
          .column = std::move(column),
        },
      });
  }
} // namespace

int main()
{
  const worm::core::MigrationPlan namePlan = planForColumn("name");
  const worm::core::MigrationPlan emailPlan = planForColumn("email");

  if (worm::core::migrationChecksum(namePlan) == 0 ||
      worm::core::migrationChecksum(namePlan) != worm::core::migrationChecksum(planForColumn("name")) ||
      worm::core::migrationChecksum(namePlan) == worm::core::migrationChecksum(emailPlan)) {
    std::cerr << "Migration checksum is not deterministic or does not change with plan contents.\n";
    return 1;
  }

  worm::core::MigrationHistory history;
  if (!history.empty() || !history.addPending("202608200001_add_user_name", namePlan) ||
      history.addPending("202608200001_add_user_name", namePlan) || history.addPending("", namePlan)) {
    std::cerr << "Migration history did not accept only unique non-empty migration ids.\n";
    return 1;
  }

  if (!history.matchesChecksum("202608200001_add_user_name", namePlan) ||
      history.matchesChecksum("202608200001_add_user_name", emailPlan) ||
      history.matchesChecksum("missing", namePlan)) {
    std::cerr << "Migration history did not validate migration checksums correctly.\n";
    return 1;
  }

  const auto appliedAt = std::chrono::system_clock::time_point{std::chrono::seconds{10}};
  if (!history.markApplied("202608200001_add_user_name", appliedAt)) {
    std::cerr << "Migration history did not mark a migration as applied.\n";
    return 1;
  }

  const worm::core::MigrationRecord* applied = history.find("202608200001_add_user_name");
  if (applied == nullptr || applied->state != worm::core::MigrationState::Applied || applied->appliedAt != appliedAt ||
      applied->rolledBackAt.has_value() || !applied->failureReason.empty()) {
    std::cerr << "Migration history did not preserve application metadata.\n";
    return 1;
  }

  if (!history.markFailed("202608200001_add_user_name", "constraint failed")) {
    std::cerr << "Migration history did not mark a migration as failed.\n";
    return 1;
  }

  const worm::core::MigrationRecord* failed = history.find("202608200001_add_user_name");
  if (failed == nullptr || failed->state != worm::core::MigrationState::Failed ||
      failed->failureReason != "constraint failed") {
    std::cerr << "Migration history did not preserve failure metadata.\n";
    return 1;
  }

  const auto rolledBackAt = std::chrono::system_clock::time_point{std::chrono::seconds{20}};
  if (!history.markRolledBack("202608200001_add_user_name", rolledBackAt)) {
    std::cerr << "Migration history did not mark a migration as rolled back.\n";
    return 1;
  }

  const worm::core::MigrationRecord* rolledBack = history.find("202608200001_add_user_name");
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

  return 0;
}
