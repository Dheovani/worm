#pragma once

#include <connection/client.hpp>

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
  };

  [[nodiscard]]
  Client& getInstance(const ConnectionConfig& connectionData, DatabaseType type);
} // namespace worm::connection
