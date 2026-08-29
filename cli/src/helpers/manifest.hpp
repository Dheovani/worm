#pragma once

#include <core/model/entity-metadata.hpp>
#include <core/model/schema-snapshot.hpp>
#include <errors/mapping-exception.hpp>

#include <concepts>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

namespace worm::cli
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

  namespace detail
  {
    template <typename T>
    concept HasEntityName = requires {
      { std::remove_cvref_t<T>::entityName() } -> std::convertible_to<std::string_view>;
    };

    template <core::PersistableEntity T>
    [[nodiscard]]
    ManifestEntity manifestEntityOf()
    {
      const core::TableMetadata metadata = core::table_metadata_of<T>();
      ManifestEntity entity;

      if constexpr (HasEntityName<T>) {
        entity.name = std::remove_cvref_t<T>::entityName();
      } else {
        entity.name = metadata.table().name();
      }

      entity.table.schema = metadata.table().schema().name();
      entity.table.name = metadata.table().name();

      entity.table.columns.reserve(metadata.columns().size());
      for (const core::ColumnMetadata& column : metadata.columns()) {
        entity.table.columns.push_back({
          .name = std::string{column.columnName},
          .type = column.type(),
          .defaultExpression =
            column.defaultExpression.empty() ? std::nullopt : std::optional<std::string>{column.defaultExpression},
          .nullable = column.nullable,
          .generated = column.generated,
          .unique = column.unique,
        });
      }

      for (const core::Column& column : metadata.primaryKey()->columns()) {
        entity.table.primaryKey.emplace_back(column.columnName);
      }

      entity.indexes.reserve(metadata.indexes().size());
      for (const core::Index& index : metadata.indexes()) {
        ManifestIndex manifestIndex{.name = std::string{index.name()}, .unique = index.unique()};
        for (const core::IndexedColumn& column : index.columns()) {
          manifestIndex.columns.push_back({std::string{column.column.columnName}, column.order});
        }
        entity.indexes.push_back(std::move(manifestIndex));
      }

      entity.foreignKeys.reserve(metadata.foreignKeys().size());
      for (const core::ForeignKey& foreignKey : metadata.foreignKeys()) {
        ManifestForeignKey manifestForeignKey{
          .name = std::string{foreignKey.name()},
          .referencedSchema = std::string{foreignKey.referencedTable().schema().name()},
          .referencedTable = std::string{foreignKey.referencedTable().name()},
          .onUpdate = foreignKey.referentialActionFor(core::Operation::Update),
          .onDelete = foreignKey.referentialActionFor(core::Operation::Delete),
        };

        for (const core::Column& column : foreignKey.columns()) {
          manifestForeignKey.columns.emplace_back(column.columnName);
        }

        for (const core::Column& column : foreignKey.referencedColumns()) {
          manifestForeignKey.referencedColumns.emplace_back(column.columnName);
        }

        entity.foreignKeys.push_back(std::move(manifestForeignKey));
      }

      return entity;
    }
  } // namespace detail

  template <core::PersistableEntity... T>
    requires(sizeof...(T) > 0)
  [[nodiscard]]
  SchemaManifest schema_manifest_of()
  {
    SchemaManifest manifest;
    manifest.entities.reserve(sizeof...(T));
    (manifest.entities.push_back(detail::manifestEntityOf<T>()), ...);

    std::unordered_set<std::string> entityNames;
    std::unordered_set<std::string> tableNames;

    for (const ManifestEntity& entity : manifest.entities) {
      if (!entityNames.insert(entity.name).second) {
        throw MappingException("Reflected schema contains duplicate entity name '{}'.", entity.name);
      }
      const std::string tableName = entity.table.schema + "." + entity.table.name;
      if (!tableNames.insert(tableName).second) {
        throw MappingException("Reflected schema contains duplicate table '{}'.", tableName);
      }
    }

    return manifest;
  }

  [[nodiscard]]
  SchemaManifest loadManifest(const std::filesystem::path& path, std::string defaultSchema);

  [[nodiscard]]
  core::SchemaMetadata schemaMetadata(const SchemaManifest& manifest);

  [[nodiscard]]
  std::string serializeManifest(const SchemaManifest& manifest);
} // namespace worm::cli
