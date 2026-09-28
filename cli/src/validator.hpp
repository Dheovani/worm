#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "parser.hpp"

namespace worm::cli
{
  void validate(const Invocation& invocation);

  [[nodiscard]]
  bool isCppKeyword(std::string_view value) noexcept;

  struct Configuration
  {
    std::optional<std::string> manifest;
    std::optional<std::string> output;
    std::optional<std::string> namespaceName;
    std::optional<std::string> driver;
    std::optional<std::string> host;
    std::optional<std::string> port;
    std::optional<std::string> database;
    std::optional<std::string> username;
    std::optional<std::string> passwordEnv;
    std::optional<std::string> migrationDirectory;
  };

  enum class ConfigurationSection
  {
    None,
    Generator,
    Database,
    Migrations
  };

  void resolve(Invocation& invocation);
} // namespace worm::cli
