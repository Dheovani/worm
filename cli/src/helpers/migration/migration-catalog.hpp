#pragma once

#include <core/model/migration-artifact.hpp>

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace worm::cli::migration
{
  struct MigrationFile
  {
    std::filesystem::path path;
    core::MigrationArtifact artifact;
  };

  struct MigrationReference
  {
    std::string id;
    std::string checksum;
  };

  enum class MigrationCatalogDifferenceKind
  {
    MissingArtifact,
    ChecksumMismatch
  };

  struct MigrationCatalogDifference
  {
    MigrationCatalogDifferenceKind kind;
    std::string id;
    std::string expectedChecksum;
    std::string actualChecksum;

    friend bool operator==(const MigrationCatalogDifference&, const MigrationCatalogDifference&) = default;
  };

  using CatalogDifferences = std::vector<MigrationCatalogDifference>;

  class MigrationCatalog
  {
  public:
    explicit MigrationCatalog(std::vector<MigrationFile> migrations = {});

    [[nodiscard]]
    const std::vector<MigrationFile>& migrations() const noexcept;

    [[nodiscard]]
    bool empty() const noexcept;

    [[nodiscard]]
    const MigrationFile* find(std::string_view id) const noexcept;

  private:
    std::vector<MigrationFile> migrations_;
  };

  [[nodiscard]]
  MigrationCatalog discoverMigrationArtifacts(const std::filesystem::path& directory);

  [[nodiscard]]
  CatalogDifferences compareCatalog(const MigrationCatalog& catalog, std::span<const MigrationReference> references);

  void validateMigrationCatalog(const MigrationCatalog& catalog, std::span<const MigrationReference> references = {});
} // namespace worm::cli::migration
