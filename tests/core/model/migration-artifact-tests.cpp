#include <core/model/migration-artifact.hpp>
#include <errors/migration-exception.hpp>

#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{
  using worm::core::MigrationArtifact;
  using worm::core::MigrationRisk;
  using worm::core::MigrationStatement;

  [[nodiscard]]
  std::vector<MigrationStatement> defaultForward()
  {
    return {{"Create users table", "CREATE TABLE users (id bigint)", MigrationRisk::Safe}};
  }

  [[nodiscard]]
  MigrationArtifact artifactWith(
    std::string id = "20260922143000",
    std::string name = "create-users",
    std::string database = "postgresql",
    std::vector<MigrationStatement> forward = defaultForward(),
    std::optional<std::vector<MigrationStatement>> rollback = std::nullopt)
  {
    return worm::core::makeMigrationArtifact(
      std::move(id),
      std::move(name),
      std::move(database),
      std::move(forward),
      std::move(rollback));
  }

  template <typename Action>
  [[nodiscard]]
  bool rejects(Action&& action)
  {
    try {
      std::forward<Action>(action)();
    } catch (const worm::MigrationException&) {
      return true;
    }
    return false;
  }
} // namespace

int main()
{
  const MigrationArtifact artifact = artifactWith();
  if (artifact.formatVersion() != MigrationArtifact::currentFormatVersion || artifact.id() != "20260922143000" ||
      artifact.name() != "create-users" || artifact.database() != "postgresql" || artifact.forward().size() != 1 ||
      artifact.rollback().has_value() ||
      artifact.checksum() != "sha256:e187208e6e2464469928ca99f15106b9b542024019430583ab926856b15be71d" ||
      !worm::core::hasValidMigrationArtifactChecksum(artifact)) {
    std::cerr << "Migration artifact did not preserve its immutable metadata and checksum.\n";
    return 1;
  }

  const MigrationArtifact equivalent = artifactWith();
  const MigrationArtifact changed = artifactWith(
    "20260922143000",
    "create-users",
    "postgresql",
    {{"Create users table", "CREATE TABLE users (id integer)", MigrationRisk::Safe}});
  const MigrationArtifact reversible = artifactWith(
    "20260922143000",
    "create-users",
    "postgresql",
    {{"Create users table", "CREATE TABLE users (id bigint)", MigrationRisk::Safe}},
    std::vector<MigrationStatement>{{"Drop users table", "DROP TABLE users", MigrationRisk::Destructive}});

  if (artifact.checksum() != equivalent.checksum() || artifact.checksum() == changed.checksum() ||
      artifact.checksum() == reversible.checksum()) {
    std::cerr << "Migration artifact checksum is not deterministic or ignored artifact contents.\n";
    return 1;
  }

  const MigrationArtifact tampered{
    artifact.formatVersion(),
    artifact.id(),
    artifact.name(),
    artifact.database(),
    artifact.checksum(),
    {{"Create users table", "CREATE TABLE administrators (id bigint)", MigrationRisk::Safe}},
  };
  const MigrationArtifact missingChecksum{
    artifact.formatVersion(),
    artifact.id(),
    artifact.name(),
    artifact.database(),
    {},
    artifact.forward(),
  };
  if (worm::core::hasValidMigrationArtifactChecksum(tampered) ||
      !rejects([&missingChecksum] { worm::core::validateMigrationArtifact(missingChecksum); }) ||
      !rejects([&tampered] { worm::core::validateMigrationArtifact(tampered); })) {
    std::cerr << "Migration artifact accepted contents that did not match the checksum.\n";
    return 1;
  }

  if (!rejects([] { static_cast<void>(artifactWith("invalid")); }) ||
      !rejects([] { static_cast<void>(artifactWith("20260922143000", "")); }) ||
      !rejects([] { static_cast<void>(artifactWith("20260922143000", "create-users", "")); }) ||
      !rejects([] { static_cast<void>(artifactWith("20260922143000", "create-users", "postgresql", {})); }) ||
      !rejects([] {
        static_cast<void>(artifactWith(
          "20260922143000",
          "create-users",
          "postgresql",
          {{"", "CREATE TABLE users (id bigint)", MigrationRisk::Safe}}));
      }) ||
      !rejects([] {
        static_cast<void>(artifactWith(
          "20260922143000",
          "create-users",
          "postgresql",
          {{"Create users table", "", MigrationRisk::Safe}}));
      }) ||
      !rejects([] {
        static_cast<void>(artifactWith(
          "20260922143000",
          "create-users",
          "postgresql",
          {{"Create users table", std::string{"CREATE\0TABLE", 12}, MigrationRisk::Safe}}));
      }) ||
      !rejects([] {
        static_cast<void>(artifactWith(
          "20260922143000",
          "create-users",
          "postgresql",
          {{"Create users table", "CREATE TABLE users (id bigint)", MigrationRisk::Safe}},
          std::vector<MigrationStatement>{}));
      })) {
    std::cerr << "Migration artifact accepted incomplete or invalid metadata.\n";
    return 1;
  }

  return 0;
}
