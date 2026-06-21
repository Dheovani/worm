#pragma once

#include <json/json.h>
#include <connection/client.hpp>

namespace worm
{
	/**
	 * This method returns a reference to the singleton database client
	 * based on the provided connection data and database type.
	 * It throws an exception if creating a new instance is not possible.
	 */
	connection::Client& GetInstance(const Json::Value& connectionData, const DbTypes& type);
}
