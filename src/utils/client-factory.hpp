#pragma once

#include <connection/client.hpp>
#include <json/json.h>

namespace worm
{
  [[nodiscard]] connection::Client& getInstance(const Json::Value& connectionData,
                                                DatabaseType type);
}
