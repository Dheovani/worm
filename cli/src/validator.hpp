#pragma once

#include "parser.hpp"

namespace worm::cli
{
  void validate(const Invocation& invocation);

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
  };

  enum class ConfigurationSection
  {
    None,
    Generator,
    Database
  };

  void resolve(Invocation& invocation);
} // namespace worm::cli
