#include "manifest.hpp"

#include <algorithm>
#include <format>
#include <fstream>
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
      return {.kind = *kind, .nativeName = name};
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

        entity.table.columns.push_back({
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

      manifest.entities.push_back(std::move(entity));
    }

    return manifest;
  }
} // namespace worm::cli::generator
