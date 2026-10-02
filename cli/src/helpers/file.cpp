#include "file.hpp"

#include <fstream>
#include <system_error>

#include <utils/logger.hpp>

#include "../errors/worm-cli-exception.hpp"

namespace worm::cli
{
  std::string readFile(const std::filesystem::path& path)
  {
    logger.log(
      LogLevel::Trace,
      "Filesystem path resolved for reading.",
      {
        {"path", path.lexically_normal().generic_string()},
      });
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
      throw WormCliException("Failed to read file '{}'.", path.string());
    }

    std::string contents{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
    logger.log(
      LogLevel::Debug,
      "File read.",
      {
        {"path", path.lexically_normal().generic_string()},
        {"bytes", std::to_string(contents.size())},
      });
    return contents;
  }

  void writeGeneratedFile(const std::filesystem::path& path, std::string_view contents)
  {
    std::error_code error;
    if (std::filesystem::exists(path, error)) {
      logger.log(
        LogLevel::Warning,
        "Existing file will not be overwritten.",
        {
          {"path", path.lexically_normal().generic_string()},
        });
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

    logger.log(
      LogLevel::Info,
      "Generated file published.",
      {
        {"path", path.lexically_normal().generic_string()},
        {"bytes", std::to_string(contents.size())},
      });
  }

  bool fileExists(const std::filesystem::path& path) noexcept
  {
    std::error_code error;
    return std::filesystem::is_regular_file(path, error);
  }

  bool fileHasContent(const std::filesystem::path& path)
  {
    std::error_code error;
    return std::filesystem::file_size(path, error) > 0 && !error;
  }
} // namespace worm::cli
