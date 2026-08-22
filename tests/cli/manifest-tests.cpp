#include <generator/manifest.hpp>

#include <errors/invalid-argument-exception.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>

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
    } catch (const worm::cli::InvalidArgumentException&) {
      return true;
    }

    return false;
  }
} // namespace

int main()
{
  const TemporaryManifest valid{
    "worm-cli-valid-manifest.json",
    R"({"version":1,"entities":[{"name":"User","table":"users","columns":[)"
    R"({"name":"id","type":"int64","nullable":false,"generated":true},{"name":"email","nullable":false,"unique":true}],)"
    R"("primaryKey":["id"]}]})",
  };
  const auto manifest = worm::cli::generator::loadManifest(valid.path(), "public");
  if (manifest.entities.size() != 1 || manifest.entities[0].name != "User" ||
      manifest.entities[0].table.schema != "public" || manifest.entities[0].table.columns.size() != 2 ||
      manifest.entities[0].table.columns[0].type.kind != worm::core::ColumnTypeKind::Int64 ||
      manifest.entities[0].table.primaryKey != std::vector<std::string>{"id"}) {
    std::cerr << "Manifest parsing failed.\n";
    return 1;
  }

  if (
    !rejects("worm-cli-invalid-manifest.json", R"({"version":1,"entities":[{"name":"User"}]})") ||
    !rejects("worm-cli-duplicate-column.json",
      R"({"version":1,"entities":[{"name":"User","table":"users","columns":[{"name":"id"},{"name":"id"}],"primaryKey":["id"]}]})") ||
    !rejects("worm-cli-unknown-primary-key.json",
      R"({"version":1,"entities":[{"name":"User","table":"users","columns":[{"name":"id"}],"primaryKey":["missing"]}]})") ||
    !rejects("worm-cli-unknown-type.json",
      R"({"version":1,"entities":[{"name":"User","table":"users","columns":[{"name":"id","type":"integer"}],"primaryKey":["id"]}]})")) {
    std::cerr << "Invalid manifest was accepted.\n";
    return 1;
  }

  return 0;
}
