#include "migration-catalog.hpp"

#include "migration-file.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

#include <errors/migration-exception.hpp>

namespace worm::cli::migration
{
  namespace
  {
    inline constexpr std::string_view migrationFileSuffix = ".worm.json";

    [[nodiscard]]
    bool migrationFileName(const std::filesystem::path& path)
    {
      return path.filename().string().ends_with(migrationFileSuffix);
    }

    [[nodiscard]]
    std::string expectedFileName(const core::MigrationArtifact& artifact)
    {
      return artifact.id() + "_" + artifact.name() + std::string{migrationFileSuffix};
    }

    void validateReferenceIds(std::span<const MigrationReference> references)
    {
      std::unordered_set<std::string_view> ids;
      ids.reserve(references.size());
      for (const MigrationReference& reference : references) {
        if (reference.id.empty() || !core::isMigrationArtifactChecksum(reference.checksum)) {
          throw MigrationException("Migration history references require a non-empty id and a SHA-256 checksum.");
        }
        if (!ids.insert(reference.id).second) {
          throw MigrationException("Migration history contains duplicate id '{}'.", reference.id);
        }
      }
    }
  } // namespace

  MigrationCatalog::MigrationCatalog(std::vector<MigrationFile> migrations)
    : migrations_(std::move(migrations))
  {
    for (const MigrationFile& migration : migrations_) {
      core::validateMigrationArtifact(migration.artifact);
      const std::string expected = expectedFileName(migration.artifact);
      if (migration.path.filename().string() != expected) {
        throw MigrationException(
          "Migration file '{}' must be named '{}'.",
          migration.path.filename().string(),
          expected);
      }
    }

    std::ranges::sort(migrations_, [](const MigrationFile& left, const MigrationFile& right) {
      if (left.artifact.id() != right.artifact.id()) {
        return left.artifact.id() < right.artifact.id();
      }
      return left.path.generic_string() < right.path.generic_string();
    });

    for (std::size_t index = 1; index < migrations_.size(); ++index) {
      if (migrations_[index - 1].artifact.id() == migrations_[index].artifact.id()) {
        throw MigrationException(
          "Migration id '{}' is declared by both '{}' and '{}'.",
          migrations_[index].artifact.id(),
          migrations_[index - 1].path.string(),
          migrations_[index].path.string());
      }
    }
  }

  const std::vector<MigrationFile>& MigrationCatalog::migrations() const noexcept
  {
    return migrations_;
  }

  bool MigrationCatalog::empty() const noexcept
  {
    return migrations_.empty();
  }

  const MigrationFile* MigrationCatalog::find(std::string_view id) const noexcept
  {
    const auto migration =
      std::ranges::lower_bound(migrations_, id, {}, [](const MigrationFile& value) -> std::string_view {
        return value.artifact.id();
      });
    if (migration == migrations_.end() || migration->artifact.id() != id) {
      return nullptr;
    }
    return &*migration;
  }

  MigrationCatalog discoverMigrationArtifacts(const std::filesystem::path& directory)
  {
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error)) {
      if (error) {
        throw MigrationException(
          "Unable to inspect migration directory '{}': {}.",
          directory.string(),
          error.message());
      }
      throw MigrationException("Migration directory '{}' does not exist or is not a directory.", directory.string());
    }

    std::vector<MigrationFile> migrations;
    std::filesystem::directory_iterator entry{directory, error};
    const std::filesystem::directory_iterator end;
    if (error) {
      throw MigrationException("Unable to read migration directory '{}': {}.", directory.string(), error.message());
    }

    while (entry != end) {
      const std::filesystem::directory_entry& file = *entry;
      if (migrationFileName(file.path())) {
        const std::filesystem::file_status status = file.symlink_status(error);
        if (error) {
          throw MigrationException("Unable to inspect migration file '{}': {}.", file.path().string(), error.message());
        }
        if (std::filesystem::is_symlink(status) || !std::filesystem::is_regular_file(status)) {
          throw MigrationException(
            "Migration path '{}' must be a regular file and cannot be a symlink.",
            file.path().string());
        }

        migrations.push_back({file.path(), loadMigrationArtifact(file.path())});
      }

      entry.increment(error);
      if (error) {
        throw MigrationException("Unable to read migration directory '{}': {}.", directory.string(), error.message());
      }
    }

    return MigrationCatalog{std::move(migrations)};
  }

  CatalogDifferences compareCatalog(const MigrationCatalog& catalog, std::span<const MigrationReference> references)
  {
    validateReferenceIds(references);

    CatalogDifferences differences;
    for (const MigrationReference& reference : references) {
      const MigrationFile* migration = catalog.find(reference.id);
      if (migration == nullptr) {
        differences.push_back(
          {
            .kind = MigrationCatalogDifferenceKind::MissingArtifact,
            .id = reference.id,
            .expectedChecksum = reference.checksum,
          });
      } else if (migration->artifact.checksum() != reference.checksum) {
        differences.push_back(
          {
            .kind = MigrationCatalogDifferenceKind::ChecksumMismatch,
            .id = reference.id,
            .expectedChecksum = reference.checksum,
            .actualChecksum = migration->artifact.checksum(),
          });
      }
    }

    std::ranges::sort(differences, [](const MigrationCatalogDifference& left, const MigrationCatalogDifference& right) {
      if (left.id != right.id) {
        return left.id < right.id;
      }
      return left.kind < right.kind;
    });
    return differences;
  }

  void validateMigrationCatalog(const MigrationCatalog& catalog, std::span<const MigrationReference> references)
  {
    const CatalogDifferences differences = compareCatalog(catalog, references);
    if (differences.empty()) {
      return;
    }

    const MigrationCatalogDifference& difference = differences.front();
    if (difference.kind == MigrationCatalogDifferenceKind::MissingArtifact) {
      throw MigrationException("Applied migration '{}' is missing from the local migration directory.", difference.id);
    }
    throw MigrationException(
      "Applied migration '{}' has checksum '{}', but the local artifact has checksum '{}'.",
      difference.id,
      difference.expectedChecksum,
      difference.actualChecksum);
  }
} // namespace worm::cli::migration
