#include <database/migration-catalog.hpp>
#include <database/migration-file.hpp>

#include <core/model/migration-artifact.hpp>
#include <errors/migration-exception.hpp>

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
  class TemporaryDirectory
  {
  public:
    TemporaryDirectory()
      : path_(std::filesystem::temp_directory_path() / "worm-cli-migration-catalog-tests")
    {
      std::error_code error;
      std::filesystem::remove_all(path_, error);
      std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory()
    {
      std::error_code error;
      std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]]
    std::filesystem::path create(std::string_view name) const
    {
      const std::filesystem::path directory = path_ / name;
      std::filesystem::create_directories(directory);
      return directory;
    }

    [[nodiscard]]
    const std::filesystem::path& path() const noexcept
    {
      return path_;
    }

  private:
    std::filesystem::path path_;
  };

  [[nodiscard]]
  worm::core::MigrationArtifact artifact(std::string id, std::string name, std::string sql)
  {
    return worm::core::makeMigrationArtifact(
      std::move(id),
      std::move(name),
      "postgresql",
      {{"Apply schema change", std::move(sql), worm::core::MigrationRisk::Safe}});
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
  using worm::cli::database::MigrationCatalogDifferenceKind;
  using worm::cli::database::MigrationReference;

  const TemporaryDirectory temporary;
  const std::filesystem::path orderedDirectory = temporary.create("ordered");
  const worm::core::MigrationArtifact later =
    artifact("20260922143002", "add-email", "ALTER TABLE users ADD COLUMN email text");
  const worm::core::MigrationArtifact earlier =
    artifact("20260922143000", "create-users", "CREATE TABLE users (id bigint)");

  worm::cli::database::saveMigrationArtifact(orderedDirectory / "20260922143002_add-email.worm.json", later);
  worm::cli::database::saveMigrationArtifact(orderedDirectory / "20260922143000_create-users.worm.json", earlier);
  std::filesystem::create_directories(orderedDirectory / "nested");
  worm::cli::database::saveMigrationArtifact(
    orderedDirectory / "nested" / "20260922143001_ignored.worm.json",
    artifact("20260922143001", "ignored", "SELECT 1"));

  const worm::cli::database::MigrationCatalog catalog =
    worm::cli::database::discoverMigrationArtifacts(orderedDirectory);
  if (catalog.empty() || catalog.migrations().size() != 2 || catalog.migrations()[0].artifact != earlier ||
      catalog.migrations()[1].artifact != later || catalog.find(earlier.id()) == nullptr ||
      catalog.find("20260922149999") != nullptr) {
    std::cerr << "Migration discovery was not non-recursive, deterministic, or searchable.\n";
    return 1;
  }

  const std::vector<MigrationReference> references{
    {.id = later.id(), .checksum = "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},
    {.id = "20260922143001", .checksum = "sha256:bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"},
  };
  const auto differences = worm::cli::database::compareCatalog(catalog, references);
  if (differences.size() != 2 || differences[0].id != "20260922143001" ||
      differences[0].kind != MigrationCatalogDifferenceKind::MissingArtifact || differences[1].id != later.id() ||
      differences[1].kind != MigrationCatalogDifferenceKind::ChecksumMismatch ||
      differences[1].expectedChecksum != "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" ||
      differences[1].actualChecksum != later.checksum()) {
    std::cerr << "Migration catalog did not report missing or edited historical artifacts.\n";
    return 1;
  }

  const std::vector<MigrationReference> matchingReferences{
    {.id = earlier.id(), .checksum = earlier.checksum()},
    {.id = later.id(), .checksum = later.checksum()},
  };
  if (!worm::cli::database::compareCatalog(catalog, matchingReferences).empty() ||
      !rejects([&] { worm::cli::database::validateMigrationCatalog(catalog, references); })) {
    std::cerr << "Migration catalog did not accept matching history or reject divergent history.\n";
    return 1;
  }
  worm::cli::database::validateMigrationCatalog(catalog, matchingReferences);

  const std::filesystem::path duplicateDirectory = temporary.create("duplicate");
  worm::cli::database::saveMigrationArtifact(
    duplicateDirectory / "20260922143003_first.worm.json",
    artifact("20260922143003", "first", "SELECT 1"));
  worm::cli::database::saveMigrationArtifact(
    duplicateDirectory / "20260922143003_second.worm.json",
    artifact("20260922143003", "second", "SELECT 2"));

  const std::filesystem::path mismatchedDirectory = temporary.create("mismatched");
  worm::cli::database::saveMigrationArtifact(
    mismatchedDirectory / "20260922143004_wrong-name.worm.json",
    artifact("20260922143004", "right-name", "SELECT 1"));

  const std::vector<MigrationReference> duplicateReferences{
    {.id = earlier.id(), .checksum = earlier.checksum()},
    {.id = earlier.id(), .checksum = earlier.checksum()},
  };
  const std::vector<MigrationReference> invalidReferences{
    {.id = earlier.id(), .checksum = "not-a-sha256-checksum"},
  };
  if (!rejects(
        [&] { static_cast<void>(worm::cli::database::discoverMigrationArtifacts(temporary.path() / "missing")); }) ||
      !rejects([&] { static_cast<void>(worm::cli::database::discoverMigrationArtifacts(duplicateDirectory)); }) ||
      !rejects([&] { static_cast<void>(worm::cli::database::discoverMigrationArtifacts(mismatchedDirectory)); }) ||
      !rejects([&] { static_cast<void>(worm::cli::database::compareCatalog(catalog, duplicateReferences)); }) ||
      !rejects([&] { static_cast<void>(worm::cli::database::compareCatalog(catalog, invalidReferences)); })) {
    std::cerr << "Migration catalog accepted a missing directory, invalid file identity, or duplicate id.\n";
    return 1;
  }

  return 0;
}
