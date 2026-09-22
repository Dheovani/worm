#pragma once

#include <core/model/migration.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace worm::core
{
  struct MigrationStatement
  {
    std::string description;
    std::string sql;
    MigrationRisk risk{MigrationRisk::Safe};

    friend bool operator==(const MigrationStatement&, const MigrationStatement&) = default;
  };

  class MigrationArtifact
  {
  public:
    static constexpr std::uint32_t currentFormatVersion = 1;

    MigrationArtifact(
      std::uint32_t formatVersion,
      std::string id,
      std::string name,
      std::string database,
      std::string checksum,
      std::vector<MigrationStatement> forward,
      std::optional<std::vector<MigrationStatement>> rollback = std::nullopt);

    [[nodiscard]]
    std::uint32_t formatVersion() const noexcept;

    [[nodiscard]]
    const std::string& id() const noexcept;

    [[nodiscard]]
    const std::string& name() const noexcept;

    [[nodiscard]]
    const std::string& database() const noexcept;

    [[nodiscard]]
    const std::string& checksum() const noexcept;

    [[nodiscard]]
    const std::vector<MigrationStatement>& forward() const noexcept;

    [[nodiscard]]
    const std::optional<std::vector<MigrationStatement>>& rollback() const noexcept;

    friend bool operator==(const MigrationArtifact&, const MigrationArtifact&) = default;

  private:
    std::uint32_t formatVersion_;
    std::string id_;
    std::string name_;
    std::string database_;
    std::string checksum_;
    std::vector<MigrationStatement> forward_;
    std::optional<std::vector<MigrationStatement>> rollback_;
  };

  [[nodiscard]]
  MigrationArtifact makeMigrationArtifact(
    std::string id,
    std::string name,
    std::string database,
    std::vector<MigrationStatement> forward,
    std::optional<std::vector<MigrationStatement>> rollback = std::nullopt);

  [[nodiscard]]
  std::string calculateMigrationArtifactChecksum(const MigrationArtifact& artifact);

  [[nodiscard]]
  bool isMigrationArtifactChecksum(std::string_view checksum) noexcept;

  [[nodiscard]]
  bool hasValidMigrationArtifactChecksum(const MigrationArtifact& artifact);

  void validateMigrationArtifact(const MigrationArtifact& artifact);
} // namespace worm::core
