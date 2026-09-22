#include "migration-file.hpp"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include <connection/client.hpp>
#include <errors/migration-exception.hpp>
#include <helpers/file.hpp>

namespace worm::cli::database
{
  namespace
  {
    using Json = nlohmann::json;

    [[nodiscard]]
    std::string_view riskName(core::MigrationRisk risk)
    {
      switch (risk) {
      case core::MigrationRisk::Safe:
        return "safe";
      case core::MigrationRisk::Ambiguous:
        return "ambiguous";
      case core::MigrationRisk::Destructive:
        return "destructive";
      }

      throw MigrationException("Migration statement has an unsupported risk value.");
    }

    [[nodiscard]]
    core::MigrationRisk parseRisk(std::string_view risk, std::string_view context)
    {
      if (risk == "safe") {
        return core::MigrationRisk::Safe;
      }
      if (risk == "ambiguous") {
        return core::MigrationRisk::Ambiguous;
      }
      if (risk == "destructive") {
        return core::MigrationRisk::Destructive;
      }

      throw MigrationException("{} has unsupported risk '{}'.", context, risk);
    }

    void ensureKnown(const Json& object, std::initializer_list<std::string_view> allowed, std::string_view context)
    {
      std::unordered_set<std::string_view> fields{allowed};
      for (auto entry = object.begin(); entry != object.end(); ++entry) {
        if (!fields.contains(entry.key())) {
          throw MigrationException("{} contains unknown field '{}'.", context, entry.key());
        }
      }
    }

    [[nodiscard]]
    std::string requiredString(const Json& object, std::string_view key, std::string_view context)
    {
      const auto value = object.find(key);
      if (value == object.end() || !value->is_string() || value->get_ref<const std::string&>().empty()) {
        throw MigrationException("{} requires a non-empty string '{}'.", context, key);
      }
      return value->get<std::string>();
    }

    [[nodiscard]]
    core::MigrationStatement parseStatement(const Json& value, std::string_view context)
    {
      if (!value.is_object()) {
        throw MigrationException("{} must be an object.", context);
      }
      ensureKnown(value, {"description", "sql", "risk"}, context);

      const std::string risk = requiredString(value, "risk", context);
      return {
        .description = requiredString(value, "description", context),
        .sql = requiredString(value, "sql", context),
        .risk = parseRisk(risk, context),
      };
    }

    [[nodiscard]]
    std::vector<core::MigrationStatement> parseSteps(const Json& value, std::string_view key, std::string_view source)
    {
      const auto values = value.find(key);
      if (values == value.end() || !values->is_array() || values->empty()) {
        throw MigrationException("Migration artifact '{}' requires a non-empty '{}' array.", source, key);
      }

      std::vector<core::MigrationStatement> statements;
      statements.reserve(values->size());
      for (std::size_t index = 0; index < values->size(); ++index) {
        statements.push_back(parseStatement(
          (*values)[index],
          "Migration artifact '" + std::string{source} + "' " + std::string{key} + " statement " +
            std::to_string(index)));
      }
      return statements;
    }

    [[nodiscard]]
    Json serializeStatement(const core::MigrationStatement& statement)
    {
      return {
        {"description", statement.description},
        {"risk", riskName(statement.risk)},
        {"sql", statement.sql},
      };
    }

    [[nodiscard]]
    bool supportedDatabase(std::string_view database) noexcept
    {
      return connection::databaseTypes.contains(std::string{database});
    }
  } // namespace

  std::string serializeMigrationArtifact(const core::MigrationArtifact& artifact)
  {
    core::validateMigrationArtifact(artifact);

    Json document{
      {"checksum", artifact.checksum()},
      {"database", artifact.database()},
      {"down", nullptr},
      {"id", artifact.id()},
      {"name", artifact.name()},
      {"up", Json::array()},
      {"version", artifact.formatVersion()},
    };

    for (const core::MigrationStatement& statement : artifact.forward()) {
      document["up"].push_back(serializeStatement(statement));
    }

    if (artifact.rollback().has_value()) {
      document["down"] = Json::array();
      for (const core::MigrationStatement& statement : *artifact.rollback()) {
        document["down"].push_back(serializeStatement(statement));
      }
    }

    return document.dump(2) + '\n';
  }

  core::MigrationArtifact parseMigrationArtifact(std::string_view contents, std::string_view source)
  {
    Json document;
    try {
      document = Json::parse(contents);
    } catch (const Json::exception& error) {
      throw MigrationException("Invalid migration artifact '{}': {}", source, error.what());
    }

    if (!document.is_object()) {
      throw MigrationException("Migration artifact '{}' must be an object.", source);
    }
    ensureKnown(document, {"version", "id", "name", "database", "checksum", "up", "down"}, source);

    const auto version = document.find("version");
    if (version == document.end() || !version->is_number_unsigned()) {
      throw MigrationException("Migration artifact '{}' requires an unsigned integer 'version'.", source);
    }

    const std::string id = requiredString(document, "id", source);
    const std::string name = requiredString(document, "name", source);
    const std::string database = requiredString(document, "database", source);
    const std::string checksum = requiredString(document, "checksum", source);
    if (!supportedDatabase(database)) {
      throw MigrationException("Migration artifact '{}' targets unsupported database '{}'.", source, database);
    }

    std::optional<std::vector<core::MigrationStatement>> rollback;
    const auto down = document.find("down");
    if (down == document.end()) {
      throw MigrationException("Migration artifact '{}' requires field 'down'; use null when unavailable.", source);
    }
    if (!down->is_null()) {
      rollback = parseSteps(document, "down", source);
    }

    core::MigrationArtifact artifact{
      version->get<std::uint32_t>(),
      id,
      name,
      database,
      checksum,
      parseSteps(document, "up", source),
      std::move(rollback),
    };
    core::validateMigrationArtifact(artifact);
    return artifact;
  }

  core::MigrationArtifact loadMigrationArtifact(const std::filesystem::path& path)
  {
    return parseMigrationArtifact(readFile(path), path.string());
  }

  void saveMigrationArtifact(const std::filesystem::path& path, const core::MigrationArtifact& artifact)
  {
    writeGeneratedFile(path, serializeMigrationArtifact(artifact));
  }
} // namespace worm::cli::database
