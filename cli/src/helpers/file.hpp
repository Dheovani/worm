#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace worm::cli
{
  [[nodiscard]]
  std::string readFile(const std::filesystem::path& path);

  void writeGeneratedFile(const std::filesystem::path& path, std::string_view contents);

  [[nodiscard]]
  bool fileExists(const std::filesystem::path& path) noexcept;

  [[nodiscard]]
  bool fileHasContent(const std::filesystem::path& path);
} // namespace worm::cli
