#include <helpers/manifest.hpp>

#include <errors/invalid-cli-argument-exception.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>

namespace
{
  class TemporaryManifest
  {
  public:
    TemporaryManifest(std::string_view name, std::string_view contents)
      : path_(std::filesystem::temp_directory_path() / name)
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

  bool rejects(std::string_view name, std::string_view contents)
  {
    const TemporaryManifest manifest{name, contents};
    try {
      static_cast<void>(worm::cli::generator::loadManifest(manifest.path(), "public"));
    } catch (const worm::cli::InvalidCliArgumentException&) {
      return true;
    }

    return false;
  }
} // namespace

int main()
{
  const TemporaryManifest valid{
    "worm-cli-valid-manifest.json",
    R"({"version":1,"entities":[{"name":"Role","table":"roles","columns":[{"name":"id","type":"int64","nullable":false}],"primaryKey":["id"]},{"name":"User","table":"users","columns":[)"
    R"({"name":"id","type":"int64","nullable":false,"generated":true},{"name":"role_id","type":"int64","nullable":false},{"name":"email","type":"string","length":120,"nullable":false,"unique":true}],)"
    R"("primaryKey":["id"],"indexes":[{"name":"idx_users_email","columns":[{"name":"email","order":"desc"}],"unique":true}],)"
    R"("foreignKeys":[{"name":"fk_users_role","columns":["role_id"],"referencedTable":"roles","referencedColumns":["id"],"onUpdate":"cascade","onDelete":"restrict"}]}]})",
  };
  const auto manifest = worm::cli::generator::loadManifest(valid.path(), "public");
  const auto& userManifest = manifest.entities[1];
  if (manifest.entities.size() != 2 || userManifest.name != "User" || userManifest.table.schema != "public" ||
      userManifest.table.columns.size() != 3 ||
      userManifest.table.columns[2].type.length != std::optional<std::size_t>{120} ||
      userManifest.table.primaryKey != std::vector<std::string>{"id"} || userManifest.indexes.size() != 1 ||
      userManifest.foreignKeys.size() != 1) {
    std::cerr << "Manifest parsing failed.\n";
    return 1;
  }

  const worm::core::SchemaMetadata metadata = worm::cli::generator::schemaMetadata(manifest);
  const auto* users = metadata.findTable(worm::core::Table{worm::core::Schema{"public"}, "users"});
  const auto* id = users == nullptr ? nullptr : users->findColumn("id");
  if (metadata.schema().name() != "public" || users == nullptr || id == nullptr ||
      id->type().kind != worm::core::ColumnTypeKind::Int64 || !id->generated || id->nullable ||
      !users->primaryKey().has_value() || users->primaryKey()->columns().front().columnName != "id" ||
      users->indexes().size() != 1 || !users->indexes().front().unique() || users->foreignKeys().size() != 1 ||
      users->foreignKeys().front().referentialActionFor(worm::core::Operation::Delete) !=
        worm::core::ReferentialAction::Restrict) {
    std::cerr << "Manifest did not convert to declarative schema metadata.\n";
    return 1;
  }

  if (
    !rejects("worm-cli-invalid-manifest.json", R"({"version":1,"entities":[{"name":"User"}]})") ||
    !rejects("worm-cli-duplicate-column.json",
      R"({"version":1,"entities":[{"name":"User","table":"users","columns":[{"name":"id"},{"name":"id"}],"primaryKey":["id"]}]})") ||
    !rejects("worm-cli-unknown-primary-key.json",
      R"({"version":1,"entities":[{"name":"User","table":"users","columns":[{"name":"id"}],"primaryKey":["missing"]}]})") ||
    !rejects("worm-cli-unknown-type.json",
      R"({"version":1,"entities":[{"name":"User","table":"users","columns":[{"name":"id","type":"integer"}],"primaryKey":["id"]}]})") ||
    !rejects("worm-cli-invalid-scale.json",
      R"({"version":1,"entities":[{"name":"User","table":"users","columns":[{"name":"id","type":"decimal","scale":2}],"primaryKey":["id"]}]})") ||
    !rejects("worm-cli-invalid-index.json",
      R"({"version":1,"entities":[{"name":"User","table":"users","columns":[{"name":"id","type":"int64"}],"primaryKey":["id"],"indexes":[{"name":"idx_users_missing","columns":["missing"]}]}]})") ||
    !rejects("worm-cli-invalid-foreign-key.json",
      R"({"version":1,"entities":[{"name":"User","table":"users","columns":[{"name":"id","type":"int64"}],"primaryKey":["id"],"foreignKeys":[{"name":"fk_users","columns":["id"],"referencedTable":"users","referencedColumns":["missing"]}]}]})")) {
    std::cerr << "Invalid manifest was accepted.\n";
    return 1;
  }

  return 0;
}
