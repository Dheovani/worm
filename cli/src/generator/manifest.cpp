#include "manifest.hpp"

#include <algorithm>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

#include <nlohmann/json.hpp>

#include "../errors/invalid-argument-exception.hpp"

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
        throw InvalidArgumentException(
          "Manifest " + std::string{context} + " requires a non-empty string '" + std::string{key} + "'.");
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
        throw InvalidArgumentException(
          "Manifest " + std::string{context} + " requires '" + std::string{key} + "' to be a boolean.");
      }

      return value->get<bool>();
    }
  } // namespace

  SchemaManifest loadManifest(const std::filesystem::path& path, std::string defaultSchema)
  {
    std::ifstream stream{path};
    if (!stream) {
      throw InvalidArgumentException("Unable to open schema manifest '" + path.string() + "'.");
    }

    Json document;
    try {
      stream >> document;
    } catch (const Json::exception& error) {
      throw InvalidArgumentException("Invalid schema manifest '" + path.string() + "': " + error.what());
    }

    if (!document.is_object() || document.value("version", 0) != 1) {
      throw InvalidArgumentException("Schema manifest must be an object with version 1.");
    }

    const auto entities = document.find("entities");
    if (entities == document.end() || !entities->is_array()) {
      throw InvalidArgumentException("Schema manifest requires an 'entities' array.");
    }

    SchemaManifest manifest;
    std::unordered_set<std::string> entityNames;
    std::unordered_set<std::string> tableNames;
    for (const Json& entityObject : *entities) {
      if (!entityObject.is_object()) {
        throw InvalidArgumentException("Every manifest entity must be an object.");
      }

      ManifestEntity entity;
      entity.name = requiredString(entityObject, "name", "entity");
      entity.table.name = requiredString(entityObject, "table", "entity '" + entity.name + "'");
      const auto schema = entityObject.find("schema");
      entity.table.schema = schema == entityObject.end()
                              ? defaultSchema
                              : requiredString(entityObject, "schema", "entity '" + entity.name + "'");

      if (!entityNames.insert(entity.name).second) {
        throw InvalidArgumentException("Manifest contains duplicate entity '" + entity.name + "'.");
      }

      const std::string tableIdentity = entity.table.schema + "." + entity.table.name;
      if (!tableNames.insert(tableIdentity).second) {
        throw InvalidArgumentException("Manifest contains duplicate table '" + tableIdentity + "'.");
      }

      const auto columns = entityObject.find("columns");
      if (columns == entityObject.end() || !columns->is_array() || columns->empty()) {
        throw InvalidArgumentException("Manifest entity '" + entity.name + "' requires a non-empty 'columns' array.");
      }

      std::unordered_set<std::string> columnNames;
      for (const Json& columnObject : *columns) {
        if (!columnObject.is_object()) {
          throw InvalidArgumentException("Every column of manifest entity '" + entity.name + "' must be an object.");
        }

        const std::string context = "column of entity '" + entity.name + "'";
        const std::string columnName = requiredString(columnObject, "name", context);
        if (!columnNames.insert(columnName).second) {
          throw InvalidArgumentException(
            "Manifest entity '" + entity.name + "' contains duplicate column '" + columnName + "'.");
        }

        entity.table.columns.push_back({
          .name = columnName,
          .nullable = optionalBoolean(columnObject, "nullable", true, context),
          .generated = optionalBoolean(columnObject, "generated", false, context),
          .unique = optionalBoolean(columnObject, "unique", false, context),
        });
      }

      const auto primaryKey = entityObject.find("primaryKey");
      if (primaryKey == entityObject.end() || !primaryKey->is_array() || primaryKey->empty()) {
        throw InvalidArgumentException("Manifest entity '" + entity.name + "' requires a primary key.");
      }

      for (const Json& column : *primaryKey) {
        if (!column.is_string() || column.get_ref<const std::string&>().empty()) {
          throw InvalidArgumentException("Manifest primary-key columns must be non-empty strings.");
        }
        const std::string columnName = column.get<std::string>();
        if (!columnNames.contains(columnName)) {
          throw InvalidArgumentException(
            "Primary-key column '" + columnName + "' does not exist in entity '" + entity.name + "'.");
        }
        if (std::find(entity.table.primaryKey.begin(), entity.table.primaryKey.end(), columnName) !=
            entity.table.primaryKey.end()) {
          throw InvalidArgumentException(
            "Manifest entity '" + entity.name + "' contains duplicate primary-key column '" + columnName + "'.");
        }
        entity.table.primaryKey.push_back(columnName);
      }

      manifest.entities.push_back(std::move(entity));
    }

    return manifest;
  }
} // namespace worm::cli::generator
