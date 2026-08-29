#include <helpers/manifest.hpp>

#include <generator/push.hpp>

#include <reflection/field.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <tuple>

namespace
{
  struct Account
  {
    std::int64_t id{};
    std::string status;

    static constexpr std::string_view entityName() noexcept
    {
      return "Account";
    }

    static constexpr worm::core::Table table() noexcept
    {
      return worm::core::Table{worm::core::Schema{"public"}, "accounts"};
    }

    static constexpr worm::core::PrimaryKey primaryKey() noexcept
    {
      return worm::core::PrimaryKey{"pk_accounts", {worm::core::Column{"id", table()}}};
    }

    static constexpr auto reflect() noexcept
    {
      return std::tuple{worm::reflection::field("id", &Account::id, {.generated = true, .nullable = false}),
        worm::reflection::field("status", &Account::status, {.defaultExpression = "'active'", .nullable = false})};
    }

    static worm::core::ColumnType columnType(std::string_view column)
    {
      if (column == "status") {
        return {
          .kind = worm::core::ColumnTypeKind::Enum,
          .enumeration =
            worm::core::NativeEnum{
              .schema = "types",
              .name = "account_status",
              .values = {"active", "blocked"},
            },
        };
      }
      return {.kind = worm::core::ColumnTypeKind::Int64};
    }

    static constexpr auto indexes() noexcept
    {
      return std::tuple{worm::core::Index{"idx_accounts_status", {{worm::core::Column{"status", table()}}}}};
    }
  };

  class TemporaryManifest final
  {
  public:
    explicit TemporaryManifest(std::string_view contents)
      : path_(std::filesystem::temp_directory_path() / "worm-reflected-entity-manifest.json")
    {
      std::ofstream{path_} << contents;
    }

    ~TemporaryManifest()
    {
      std::error_code error;
      std::filesystem::remove(path_, error);
    }

    [[nodiscard]]
    const std::filesystem::path& path() const noexcept
    {
      return path_;
    }

  private:
    std::filesystem::path path_;
  };
} // namespace

int main()
{
  const worm::cli::SchemaManifest reflected = worm::cli::schema_manifest_of<Account>();
  if (reflected.entities.size() != 1 || reflected.entities[0].name != "Account" ||
      reflected.entities[0].table.name != "accounts" || reflected.entities[0].table.columns.size() != 2 ||
      reflected.entities[0].table.columns[1].type.kind != worm::core::ColumnTypeKind::Enum ||
      reflected.entities[0].indexes.size() != 1) {
    std::cerr << "Reflected entities did not produce a schema manifest.\n";
    return 1;
  }

  const std::string serialized = worm::cli::serializeManifest(reflected);
  if (serialized.find("\"enumName\": \"account_status\"") == std::string::npos ||
      serialized.find("\"enumSchema\": \"types\"") == std::string::npos ||
      serialized.find("\"values\"") == std::string::npos ||
      serialized.find("\"idx_accounts_status\"") == std::string::npos) {
    std::cerr << "Reflected schema manifest serialization omitted metadata.\n";
    return 1;
  }

  const TemporaryManifest file{serialized};
  const worm::cli::SchemaManifest loaded = worm::cli::loadManifest(file.path(), "public");
  const worm::core::SchemaMetadata metadata = worm::cli::schemaMetadata(loaded);
  const worm::core::TableMetadata* accounts = metadata.findTable(Account::table());
  const worm::core::ColumnMetadata* status = accounts == nullptr ? nullptr : accounts->findColumn("status");
  if (loaded.entities.size() != 1 || accounts == nullptr || status == nullptr ||
      status->type().enumeration->values != std::vector<std::string>{"active", "blocked"} ||
      status->type().enumeration->schema != "types" || accounts->indexes().size() != 1) {
    std::cerr << "Serialized reflected schema did not survive a manifest round trip.\n";
    return 1;
  }

  worm::cli::SchemaManifest schemaless = reflected;
  schemaless.entities[0].table.schema.clear();
  const std::string schemalessSerialized = worm::cli::serializeManifest(schemaless);
  if (schemalessSerialized.find("\"schema\":") != std::string::npos) {
    std::cerr << "Schemaless manifest serialized an invalid empty schema.\n";
    return 1;
  }
  const TemporaryManifest schemalessFile{schemalessSerialized};
  const worm::cli::SchemaManifest schemalessLoaded = worm::cli::loadManifest(schemalessFile.path(), "application");
  if (schemalessLoaded.entities[0].table.schema != "application" ||
      schemalessLoaded.entities[0].table.columns[1].type.enumeration->schema != "types") {
    std::cerr << "Manifest defaults or enum schema were not preserved.\n";
    return 1;
  }

  const worm::cli::Invocation invocation{.command = worm::cli::Commands::Push};
  const worm::cli::ExecutionReport plan = worm::cli::generator::planPush(invocation, reflected, {});
  const auto metrics = std::dynamic_pointer_cast<const worm::cli::generator::PushMetrics>(plan.metrics);
  if (plan.status != worm::cli::ExecutionStatus::Success || metrics == nullptr || metrics->entitiesSelected != 1 ||
      metrics->missingTables != 1 || metrics->plannedTables != 1) {
    std::cerr << "Reflected schema manifest did not enter the push planning flow.\n";
    return 1;
  }

  return 0;
}
