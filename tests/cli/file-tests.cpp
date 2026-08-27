#include <helpers/file.hpp>

#include <errors/worm-cli-exception.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

int main()
{
  const std::filesystem::path directory = std::filesystem::temp_directory_path() / "worm-cli-file-tests";
  const std::filesystem::path file = directory / "generated" / "schema.sql";
  std::error_code error;
  std::filesystem::remove_all(directory, error);

  try {
    worm::cli::writeGeneratedFile(file, "create table users (id integer);\n");

    std::ifstream stream{file};
    std::ostringstream contents;
    contents << stream.rdbuf();
    if (!stream || contents.str() != "create table users (id integer);\n" ||
        std::filesystem::exists(file.string() + ".worm-tmp")) {
      std::cerr << "Generated file was not published atomically.\n";
      std::filesystem::remove_all(directory, error);
      return 1;
    }

    try {
      worm::cli::writeGeneratedFile(file, "replacement");
      std::cerr << "Generated file helper overwrote an existing file.\n";
      std::filesystem::remove_all(directory, error);
      return 1;
    } catch (const worm::cli::WormCliException&) {}
  } catch (const std::exception& exception) {
    std::cerr << "Generated file helper failed: " << exception.what() << '\n';
    std::filesystem::remove_all(directory, error);
    return 1;
  }

  std::filesystem::remove_all(directory, error);
  return 0;
}
