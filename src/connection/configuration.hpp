#pragma once

#include <connection/client.hpp>

#include <chrono>
#include <memory>
#include <optional>
#include <string>

namespace worm::connection
{
  struct TimeoutConfig
  {
    std::optional<std::chrono::milliseconds> connectionTimeout;
    std::optional<std::chrono::milliseconds> queryTimeout;
    bool cancelOnTimeout{true};
  };

  struct ConnectionConfig
  {
    std::string host;
    std::string username;
    std::string password;
    std::string dbname;
    std::string port;
    bool cacheResults{false};
    TimeoutConfig timeoutConfig;
  };

  [[nodiscard]]
  std::unique_ptr<Client> makeClient(const ConnectionConfig& connectionData, DatabaseType type);

  [[nodiscard]]
  std::chrono::milliseconds timeoutMilliseconds(std::chrono::milliseconds timeout);

  [[nodiscard]]
  std::chrono::seconds timeoutSeconds(std::chrono::milliseconds timeout);
} // namespace worm::connection
