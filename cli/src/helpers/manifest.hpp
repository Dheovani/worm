#pragma once

#include <core/model/schema-metadata.hpp>
#include <core/model/schema-snapshot.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace worm::cli::generator
{
  struct ManifestIndexColumn
  {
    std::string name;
    core::IndexOrder order{core::IndexOrder::Ascending};
  };

  struct ManifestIndex
  {
    std::string name;
    std::vector<ManifestIndexColumn> columns;
    bool unique{false};
  };

  struct ManifestForeignKey
  {
    std::string name;
    std::vector<std::string> columns;
    std::string referencedSchema;
    std::string referencedTable;
    std::vector<std::string> referencedColumns;
    core::ReferentialAction onUpdate{core::ReferentialAction::NoAction};
    core::ReferentialAction onDelete{core::ReferentialAction::NoAction};
  };

  struct ManifestEntity
  {
    std::string name;
    core::SchemaTableSnapshot table;
    std::vector<ManifestIndex> indexes;
    std::vector<ManifestForeignKey> foreignKeys;
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
