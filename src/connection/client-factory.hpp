#pragma once

#include <connection/client.hpp>

#include <json/json.h>

namespace worm::connection
{
  [[nodiscard]]
  Client& getInstance(const Json::Value& connectionData, DatabaseType type);
} // namespace worm::connection
