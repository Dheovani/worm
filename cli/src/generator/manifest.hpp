#pragma once

#include <core/model/schema-metadata.hpp>
#include <core/model/schema-snapshot.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace worm::cli::generator
{
  struct ManifestEntity
  {
    std::string name;
    core::SchemaTableSnapshot table;
  };

  struct SchemaManifest
  {
    int version{1};
    std::vector<ManifestEntity> entities;
  };

  [[nodiscard]]
  SchemaManifest loadManifest(const std::filesystem::path& path, std::string defaultSchema);

  [[nodiscard]]
  core::SchemaMetadata schemaMetadata(const SchemaManifest& manifest);
} // namespace worm::cli::generator
