#pragma once

#include <connection/client.hpp>

#include <memory>
#include <string>

namespace worm::connection
{
  struct ConnectionConfig
  {
    std::string host;
    std::string username;
    std::string password;
    std::string dbname;
    std::string port;
    bool cacheResults{false};
  };

  [[nodiscard]]
  std::unique_ptr<Client> makeClient(const ConnectionConfig& connectionData, DatabaseType type);
} // namespace worm::connection
