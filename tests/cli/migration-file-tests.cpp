#include <database/migration-file.hpp>

#include <core/model/migration-artifact.hpp>
#include <errors/migration-exception.hpp>
#include <errors/worm-exception.hpp>

#include <filesystem>
#include <fstream>
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
      : path_(std::filesystem::temp_directory_path() / "worm-cli-migration-file-tests")
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
    const std::filesystem::path& path() const noexcept
    {
      return path_;
    }

  private:
    std::filesystem::path path_;
  };

  template <typename Action>
  [[nodiscard]]
  bool rejects(Action&& action)
  {
    try {
      std::forward<Action>(action)();
    } catch (const worm::WormException&) {
      return true;
    }
    return false;
  }
} // namespace

int main()
{
  using worm::core::MigrationRisk;
  using worm::core::MigrationStatement;

  const worm::core::MigrationArtifact artifact = worm::core::makeMigrationArtifact(
    "20260922143000",
    "create-users",
    "postgresql",
    std::vector<MigrationStatement>{
      {"Create account status", "CREATE TYPE account_status AS ENUM ('active', 'blocked')", MigrationRisk::Safe},
      {"Create users table", "CREATE TABLE users (id bigint PRIMARY KEY)", MigrationRisk::Safe},
    },
    std::vector<MigrationStatement>{
      {"Drop users table", "DROP TABLE users", MigrationRisk::Destructive},
      {"Drop account status", "DROP TYPE account_status", MigrationRisk::Destructive},
    });

  const std::string serialized = worm::cli::database::serializeMigrationArtifact(artifact);
  const worm::core::MigrationArtifact parsed =
    worm::cli::database::parseMigrationArtifact(serialized, "in-memory migration");
  if (parsed != artifact || worm::cli::database::serializeMigrationArtifact(parsed) != serialized ||
      serialized.find("\"down\": [") == std::string::npos ||
      serialized.find("\"risk\": \"destructive\"") == std::string::npos) {
    std::cerr << "Migration artifact did not survive canonical JSON serialization.\n";
    return 1;
  }

  const TemporaryDirectory temporary;
  const std::filesystem::path path = temporary.path() / "20260922143000_create-users.worm.json";
  worm::cli::database::saveMigrationArtifact(path, artifact);
  if (worm::cli::database::loadMigrationArtifact(path) != artifact ||
      !rejects([&] { worm::cli::database::saveMigrationArtifact(path, artifact); })) {
    std::cerr << "Migration artifact file was not written, loaded, or protected from overwrite.\n";
    return 1;
  }

  std::string tampered = serialized;
  tampered.replace(tampered.find("users (id bigint"), std::string_view{"users (id bigint"}.size(), "admins (id int");

  const std::vector<std::string> invalidArtifacts{
    "not json",
    R"([])",
    R"json({
      "version": 1,
      "id": "20260922143000",
      "name": "create-users",
      "database": "postgresql",
      "checksum": "invalid",
      "up": [],
      "down": null
    })json",
    R"json({
      "version": 2,
      "id": "20260922143000",
      "name": "create-users",
      "database": "postgresql",
      "checksum": "invalid",
      "up": [{
        "description": "Create users",
        "sql": "CREATE TABLE users (id bigint)",
        "risk": "safe"
      }],
      "down": null
    })json",
    R"json({
      "version": 1,
      "id": "20260922143000",
      "name": "create-users",
      "database": "oracle",
      "checksum": "invalid",
      "up": [{
        "description": "Create users",
        "sql": "CREATE TABLE users (id bigint)",
        "risk": "safe"
      }],
      "down": null
    })json",
    R"json({
      "version": 1,
      "id": "20260922143000",
      "name": "create-users",
      "database": "postgresql",
      "checksum": "invalid",
      "up": [{
        "description": "Create users",
        "sql": "CREATE TABLE users (id bigint)",
        "risk": "unknown"
      }],
      "down": null
    })json",
    R"json({
      "version": 1,
      "id": "20260922143000",
      "name": "create-users",
      "database": "postgresql",
      "checksum": "invalid",
      "up": [{
        "description": "Create users",
        "sql": "CREATE TABLE users (id bigint)",
        "risk": "safe"
      }],
      "down": [],
      "extra": true
    })json",
  };

  if (!rejects([&] { static_cast<void>(worm::cli::database::parseMigrationArtifact(tampered)); })) {
    std::cerr << "Migration artifact parser accepted modified contents with a stale checksum.\n";
    return 1;
  }
  for (const std::string& contents : invalidArtifacts) {
    if (!rejects([&] { static_cast<void>(worm::cli::database::parseMigrationArtifact(contents)); })) {
      std::cerr << "Migration artifact parser accepted malformed or unsupported JSON.\n";
      return 1;
    }
  }

  return 0;
}
