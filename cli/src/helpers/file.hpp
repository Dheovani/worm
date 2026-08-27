#pragma once

#include <filesystem>
#include <string_view>

namespace worm::cli
{
  void writeGeneratedFile(const std::filesystem::path& path, std::string_view contents);
} // namespace worm::cli
