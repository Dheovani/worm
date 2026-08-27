#include "file.hpp"

#include <fstream>
#include <system_error>

#include "../errors/worm-cli-exception.hpp"

namespace worm::cli
{
  void writeGeneratedFile(const std::filesystem::path& path, std::string_view contents)
  {
    std::error_code error;
    if (std::filesystem::exists(path, error)) {
      throw WormCliException("Generated file '{}' already exists and will not be overwritten.", path.string());
    }
    if (error) {
      throw WormCliException("Unable to inspect generated file '{}': {}.", path.string(), error.message());
    }

    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty()) {
      std::filesystem::create_directories(parent, error);
      if (error) {
        throw WormCliException("Failed to create output directory '{}': {}.", parent.string(), error.message());
      }
    }

    const std::filesystem::path temporary = path.string() + ".worm-tmp";
    std::ofstream stream{temporary, std::ios::binary | std::ios::trunc};
    if (!stream || !(stream << contents)) {
      std::filesystem::remove(temporary, error);
      throw WormCliException("Failed to write generated file '{}'.", path.string());
    }

    stream.close();
    if (!stream) {
      std::filesystem::remove(temporary, error);
      throw WormCliException("Failed to close generated file '{}'.", path.string());
    }

    std::filesystem::rename(temporary, path, error);
    if (error) {
      std::filesystem::remove(temporary, error);
      throw WormCliException("Failed to publish generated file '{}'.", path.string());
    }
  }
} // namespace worm::cli
