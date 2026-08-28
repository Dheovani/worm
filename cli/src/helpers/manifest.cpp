#include "manifest.hpp"

#include <algorithm>
#include <format>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

#include <nlohmann/json.hpp>

#include "../errors/invalid-cli-argument-exception.hpp"

namespace worm::cli::generator
{
  namespace
  {
    using Json = nlohmann::json;

    [[nodiscard]]
    std::string requiredString(const Json& object, std::string_view key, std::string_view context)
    {
      const auto value = object.find(key);
      if (value == object.end() || !value->is_string() || value->get_ref<const std::string&>().empty()) {
        throw InvalidCliArgumentException("Manifest {} requires a non-empty string '{}'.", context, key);
      }

      return value->get<std::string>();
    }

    [[nodiscard]]
    bool optionalBoolean(const Json& object, std::string_view key, bool fallback, std::string_view context)
    {
      const auto value = object.find(key);
      if (value == object.end()) {
        return fallback;
      }

      if (!value->is_boolean()) {
        throw InvalidCliArgumentException("Manifest {} requires '{}' to be a boolean.", context, key);
      }

      return value->get<bool>();
    }

    [[nodiscard]]
    std::optional<std::size_t> optionalSize(const Json& object, std::string_view key, std::string_view context)
    {
      const auto value = object.find(key);
      if (value == object.end()) {
        return std::nullopt;
      }

      if (!value->is_number_unsigned()) {
        throw InvalidCliArgumentException("Manifest {} requires '{}' to be a non-negative integer.", context, key);
      }

      return value->get<std::size_t>();
    }

    [[nodiscard]]
    core::ColumnType optionalColumnType(const Json& object, std::string_view context)
    {
      const auto value = object.find("type");
      if (value == object.end()) {
        return {};
      }
      if (!value->is_string()) {
        throw InvalidCliArgumentException("Manifest {} requires 'type' to be a string.", context);
      }

      const std::string name = value->get<std::string>();
      const auto kind = core::parseColumnTypeKind(name);
      if (!kind.has_value()) {
        throw InvalidCliArgumentException("Manifest {} has unknown column type '{}'.", context, name);
      }
      core::ColumnType type{
        .kind = *kind,
        .nativeName = name,
        .length = optionalSize(object, "length", context),
        .precision = optionalSize(object, "precision", context),
        .scale = optionalSize(object, "scale", context),
        .unsignedValue = optionalBoolean(object, "unsigned", false, context),
        .withTimeZone = optionalBoolean(object, "withTimeZone", false, context),
      };

      if (type.length == 0) {
        throw InvalidCliArgumentException("Manifest {} requires 'length' to be greater than zero.", context);
      }

      if (type.scale.has_value() && !type.precision.has_value()) {
        throw InvalidCliArgumentException("Manifest {} cannot define 'scale' without 'precision'.", context);
      }

      if (type.precision == 0 || type.scale.value_or(0) > type.precision.value_or(0)) {
        throw InvalidCliArgumentException("Manifest {} has invalid decimal precision or scale.", context);
      }

      return type;
    }

    [[nodiscard]]
    std::vector<std::string> requiredColumnList(
      const Json& object,
      std::string_view key,
      std::string_view context,
      const std::unordered_set<std::string>* knownColumns = nullptr)
    {
      const auto columns = object.find(key);
      if (columns == object.end() || !columns->is_array() || columns->empty()) {
        throw InvalidCliArgumentException("Manifest {} requires a non-empty '{}' array.", context, key);
      }

      std::vector<std::string> result;
      std::unordered_set<std::string> uniqueColumns;
      result.reserve(columns->size());
      for (const Json& column : *columns) {
        if (!column.is_string() || column.get_ref<const std::string&>().empty()) {
          throw InvalidCliArgumentException("Manifest {} requires '{}' to contain non-empty strings.", context, key);
        }

        std::string name = column.get<std::string>();
        if (knownColumns != nullptr && !knownColumns->contains(name)) {
          throw InvalidCliArgumentException("Manifest {} references unknown column '{}'.", context, name);
        }
        if (!uniqueColumns.insert(name).second) {
          throw InvalidCliArgumentException("Manifest {} contains duplicate column '{}'.", context, name);
        }
        result.push_back(std::move(name));
      }
      return result;
    }

    [[nodiscard]]
    core::IndexOrder indexOrder(const Json& object, std::string_view context)
    {
      const auto order = object.find("order");
      if (order == object.end() || (order->is_string() && order->get<std::string>() == "asc")) {
        return core::IndexOrder::Ascending;
      }
      if (order->is_string() && order->get<std::string>() == "desc") {
        return core::IndexOrder::Descending;
      }
      throw InvalidCliArgumentException("Manifest {} requires 'order' to be 'asc' or 'desc'.", context);
    }

    [[nodiscard]]
    core::ReferentialAction referentialAction(const Json& object, std::string_view key, std::string_view context)
    {
      const auto value = object.find(key);
      if (value == object.end()) {
        return core::ReferentialAction::NoAction;
      }
      if (!value->is_string()) {
        throw InvalidCliArgumentException("Manifest {} requires '{}' to be a string.", context, key);
      }

      const std::string action = value->get<std::string>();
      if (action == "no-action") {
        return core::ReferentialAction::NoAction;
      }
      if (action == "restrict") {
        return core::ReferentialAction::Restrict;
      }
      if (action == "cascade") {
        return core::ReferentialAction::Cascade;
      }
      if (action == "set-null") {
        return core::ReferentialAction::SetNull;
      }
      if (action == "set-default") {
        return core::ReferentialAction::SetDefault;
      }
      throw InvalidCliArgumentException("Manifest {} has unsupported referential action '{}'.", context, action);
    }
  } // namespace

  SchemaManifest loadManifest(const std::filesystem::path& path, std::string defaultSchema)
  {
    std::ifstream stream{path};
    if (!stream) {
      throw InvalidCliArgumentException("Unable to open schema manifest '{}'.", path.string());
    }

    Json document;
    try {
      stream >> document;
    } catch (const Json::exception& error) {
      throw InvalidCliArgumentException("Invalid schema manifest '{}': {}", path.string(), error.what());
    }

    if (!document.is_object() || document.value("version", 0) != 1) {
      throw InvalidCliArgumentException("Schema manifest must be an object with version 1.");
    }

    const auto entities = document.find("entities");
    if (entities == document.end() || !entities->is_array()) {
      throw InvalidCliArgumentException("Schema manifest requires an 'entities' array.");
    }

    SchemaManifest manifest;
    std::unordered_set<std::string> entityNames;
    std::unordered_set<std::string> tableNames;
    for (const Json& entityObject : *entities) {
      if (!entityObject.is_object()) {
        throw InvalidCliArgumentException("Every manifest entity must be an object.");
      }

      ManifestEntity entity;
      entity.name = requiredString(entityObject, "name", "entity");
      entity.table.name = requiredString(entityObject, "table", std::format("entity '{}'", entity.name));
      const auto schema = entityObject.find("schema");
      entity.table.schema = schema == entityObject.end()
                              ? defaultSchema
                              : requiredString(entityObject, "schema", std::format("entity '{}'", entity.name));

      if (!entityNames.insert(entity.name).second) {
        throw InvalidCliArgumentException("Manifest contains duplicate entity '{}'.", entity.name);
      }

      const std::string tableIdentity = entity.table.schema + "." + entity.table.name;
      if (!tableNames.insert(tableIdentity).second) {
        throw InvalidCliArgumentException("Manifest contains duplicate table '{}'.", tableIdentity);
      }

      const auto columns = entityObject.find("columns");
      if (columns == entityObject.end() || !columns->is_array() || columns->empty()) {
        throw InvalidCliArgumentException("Manifest entity '{}' requires a non-empty 'columns' array.", entity.name);
      }

      std::unordered_set<std::string> columnNames;
      for (const Json& columnObject : *columns) {
        if (!columnObject.is_object()) {
          throw InvalidCliArgumentException("Every column of manifest entity '{}' must be an object.", entity.name);
        }

        const std::string context = std::format("column of entity '{}'", entity.name);
        const std::string columnName = requiredString(columnObject, "name", context);
        if (!columnNames.insert(columnName).second) {
          throw InvalidCliArgumentException(
            "Manifest entity '{}' contains duplicate column '{}'.",
            entity.name,
            columnName);
        }

        entity.table.columns.push_back(
          {
            .name = columnName,
            .type = optionalColumnType(columnObject, context),
            .nullable = optionalBoolean(columnObject, "nullable", true, context),
            .generated = optionalBoolean(columnObject, "generated", false, context),
            .unique = optionalBoolean(columnObject, "unique", false, context),
          });
      }

      const auto primaryKey = entityObject.find("primaryKey");
      if (primaryKey == entityObject.end() || !primaryKey->is_array() || primaryKey->empty()) {
        throw InvalidCliArgumentException("Manifest entity '{}' requires a primary key.", entity.name);
      }

      for (const Json& column : *primaryKey) {
        if (!column.is_string() || column.get_ref<const std::string&>().empty()) {
          throw InvalidCliArgumentException("Manifest primary-key columns must be non-empty strings.");
        }
        const std::string columnName = column.get<std::string>();
        if (!columnNames.contains(columnName)) {
          throw InvalidCliArgumentException(
            "Primary-key column '{}' does not exist in entity '{}'.",
            columnName,
            entity.name);
        }
        if (std::find(entity.table.primaryKey.begin(), entity.table.primaryKey.end(), columnName) !=
            entity.table.primaryKey.end()) {
          throw InvalidCliArgumentException(
            "Manifest entity '{}' contains duplicate primary-key column '{}'.",
            entity.name,
            columnName);
        }
        entity.table.primaryKey.push_back(columnName);
      }

      const auto indexes = entityObject.find("indexes");
      if (indexes != entityObject.end()) {
        if (!indexes->is_array()) {
          throw InvalidCliArgumentException("Manifest entity '{}' requires 'indexes' to be an array.", entity.name);
        }

        std::unordered_set<std::string> indexNames;
        for (const Json& indexObject : *indexes) {
          if (!indexObject.is_object()) {
            throw InvalidCliArgumentException("Every index of manifest entity '{}' must be an object.", entity.name);
          }

          const std::string context = std::format("index of entity '{}'", entity.name);
          ManifestIndex index{
            .name = requiredString(indexObject, "name", context),
            .unique = optionalBoolean(indexObject, "unique", false, context),
          };
          if (!indexNames.insert(index.name).second) {
            throw InvalidCliArgumentException(
              "Manifest entity '{}' contains duplicate index '{}'.",
              entity.name,
              index.name);
          }

          const auto indexColumns = indexObject.find("columns");
          if (indexColumns == indexObject.end() || !indexColumns->is_array() || indexColumns->empty()) {
            throw InvalidCliArgumentException("Manifest {} requires a non-empty 'columns' array.", context);
          }

          std::unordered_set<std::string> indexedColumns;
          for (const Json& indexedColumn : *indexColumns) {
            ManifestIndexColumn column;
            if (indexedColumn.is_string()) {
              column.name = indexedColumn.get<std::string>();
            } else if (indexedColumn.is_object()) {
              column.name = requiredString(indexedColumn, "name", context);
              column.order = indexOrder(indexedColumn, context);
            } else {
              throw InvalidCliArgumentException("Manifest {} has an invalid indexed column.", context);
            }

            if (column.name.empty() || !columnNames.contains(column.name)) {
              throw InvalidCliArgumentException("Manifest {} references unknown column '{}'.", context, column.name);
            }
            if (!indexedColumns.insert(column.name).second) {
              throw InvalidCliArgumentException("Manifest {} contains duplicate column '{}'.", context, column.name);
            }
            index.columns.push_back(std::move(column));
          }
          entity.indexes.push_back(std::move(index));
        }
      }

      const auto foreignKeys = entityObject.find("foreignKeys");
      if (foreignKeys != entityObject.end()) {
        if (!foreignKeys->is_array()) {
          throw InvalidCliArgumentException("Manifest entity '{}' requires 'foreignKeys' to be an array.", entity.name);
        }

        std::unordered_set<std::string> foreignKeyNames;
        for (const Json& foreignKeyObject : *foreignKeys) {
          if (!foreignKeyObject.is_object()) {
            throw InvalidCliArgumentException(
              "Every foreign key of manifest entity '{}' must be an object.",
              entity.name);
          }

          const std::string context = std::format("foreign key of entity '{}'", entity.name);
          ManifestForeignKey foreignKey{
            .name = requiredString(foreignKeyObject, "name", context),
            .columns = requiredColumnList(foreignKeyObject, "columns", context, &columnNames),
            .referencedSchema = entity.table.schema,
            .referencedTable = requiredString(foreignKeyObject, "referencedTable", context),
            .referencedColumns = requiredColumnList(foreignKeyObject, "referencedColumns", context),
            .onUpdate = referentialAction(foreignKeyObject, "onUpdate", context),
            .onDelete = referentialAction(foreignKeyObject, "onDelete", context),
          };

          const auto referencedSchema = foreignKeyObject.find("referencedSchema");
          if (referencedSchema != foreignKeyObject.end()) {
            foreignKey.referencedSchema = requiredString(foreignKeyObject, "referencedSchema", context);
          }
          if (foreignKey.columns.size() != foreignKey.referencedColumns.size()) {
            throw InvalidCliArgumentException(
              "Manifest {} requires matching local and referenced column counts.",
              context);
          }
          if (!foreignKeyNames.insert(foreignKey.name).second) {
            throw InvalidCliArgumentException(
              "Manifest entity '{}' contains duplicate foreign key '{}'.",
              entity.name,
              foreignKey.name);
          }
          entity.foreignKeys.push_back(std::move(foreignKey));
        }
      }

      manifest.entities.push_back(std::move(entity));
    }

    for (const ManifestEntity& entity : manifest.entities) {
      for (const ManifestForeignKey& foreignKey : entity.foreignKeys) {
        const auto referencedEntity =
          std::find_if(manifest.entities.begin(), manifest.entities.end(), [&](const auto& candidate) {
            return candidate.table.schema == foreignKey.referencedSchema &&
                   candidate.table.name == foreignKey.referencedTable;
          });
        if (referencedEntity == manifest.entities.end()) {
          continue;
        }

        for (const std::string& referencedColumn : foreignKey.referencedColumns) {
          if (referencedEntity->table.findColumn(referencedColumn) == nullptr) {
            throw InvalidCliArgumentException(
              "Foreign key '{}' of entity '{}' references unknown column '{}.{}'.",
              foreignKey.name,
              entity.name,
              foreignKey.referencedTable,
              referencedColumn);
          }
        }
      }
    }

    return manifest;
  }

  core::SchemaMetadata schemaMetadata(const SchemaManifest& manifest)
  {
    std::vector<core::TableMetadata> tables;
    tables.reserve(manifest.entities.size());

    for (const ManifestEntity& entity : manifest.entities) {
      const core::Schema schema{entity.table.schema};
      const core::Table table{schema, entity.table.name};
      std::vector<core::ColumnMetadata> columns;
      columns.reserve(entity.table.columns.size());
      for (const core::SchemaColumnSnapshot& column : entity.table.columns) {
        columns.emplace_back(
          core::Column{
            reflection::FieldMetadata{
              .columnName = column.name,
              .generated = column.generated,
              .unique = column.unique,
              .nullable = column.nullable,
            },
            table,
          },
          column.type);
      }

      std::vector<core::Column> primaryKeyColumns;
      primaryKeyColumns.reserve(entity.table.primaryKey.size());
      for (const std::string& columnName : entity.table.primaryKey) {
        primaryKeyColumns.emplace_back(columnName, table);
      }

      std::vector<core::Index> indexes;
      indexes.reserve(entity.indexes.size());
      for (const ManifestIndex& manifestIndex : entity.indexes) {
        std::vector<core::IndexedColumn> indexedColumns;
        indexedColumns.reserve(manifestIndex.columns.size());
        for (const ManifestIndexColumn& column : manifestIndex.columns) {
          indexedColumns.push_back({core::Column{column.name, table}, column.order});
        }
        indexes.emplace_back(
          manifestIndex.name,
          std::span<const core::IndexedColumn>{indexedColumns},
          manifestIndex.unique);
      }

      std::vector<core::ForeignKey> foreignKeys;
      foreignKeys.reserve(entity.foreignKeys.size());
      for (const ManifestForeignKey& manifestForeignKey : entity.foreignKeys) {
        std::vector<core::Column> localColumns;
        localColumns.reserve(manifestForeignKey.columns.size());
        for (const std::string& columnName : manifestForeignKey.columns) {
          localColumns.emplace_back(columnName, table);
        }

        const core::Table referencedTable{core::Schema{manifestForeignKey.referencedSchema},
          manifestForeignKey.referencedTable};
        std::vector<core::Column> referencedColumns;
        referencedColumns.reserve(manifestForeignKey.referencedColumns.size());
        for (const std::string& columnName : manifestForeignKey.referencedColumns) {
          referencedColumns.emplace_back(columnName, referencedTable);
        }

        std::vector<core::ReferentialActionEntry> actions;
        if (manifestForeignKey.onUpdate != core::ReferentialAction::NoAction) {
          actions.push_back({core::Operation::Update, manifestForeignKey.onUpdate});
        }
        if (manifestForeignKey.onDelete != core::ReferentialAction::NoAction) {
          actions.push_back({core::Operation::Delete, manifestForeignKey.onDelete});
        }

        foreignKeys.emplace_back(
          manifestForeignKey.name,
          std::span<const core::Column>{localColumns},
          referencedTable,
          std::span<const core::Column>{referencedColumns},
          std::span<const core::ReferentialActionEntry>{actions});
      }

      tables.emplace_back(
        table,
        std::move(columns),
        core::PrimaryKey{"", std::span<const core::Column>{primaryKeyColumns}},
        std::move(indexes),
        std::move(foreignKeys));
    }

    const core::Schema schema = manifest.entities.empty() ? core::Schema{} : tables.front().table().schema();
    return core::SchemaMetadata{schema, std::move(tables)};
  }
} // namespace worm::cli::generator
