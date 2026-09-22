#pragma once

#include <core/model/migration-artifact.hpp>

#include <filesystem>
#include <string>
#include <string_view>

namespace worm::cli::database
{
  [[nodiscard]]
  std::string serializeMigrationArtifact(const core::MigrationArtifact& artifact);

  [[nodiscard]]
  core::MigrationArtifact parseMigrationArtifact(std::string_view contents, std::string_view source = "migration");

  [[nodiscard]]
  core::MigrationArtifact loadMigrationArtifact(const std::filesystem::path& path);

  void saveMigrationArtifact(const std::filesystem::path& path, const core::MigrationArtifact& artifact);
} // namespace worm::cli::database
