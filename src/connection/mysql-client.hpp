#pragma once

#include <json/json.h>
#include <mysql/mysql.h>
#include <connection/client.hpp>

namespace worm
{
namespace connection
{
	class MySqlClient final : public Client
	{
	private:
		MYSQL* conn; // Pointer to the MySQL connection.

		MySqlClient(const char* host, const char* user, const char* passwd, const char* db, unsigned int port);

		// Prevent the use of copy constructor and assignment operator for safety.
		MySqlClient(const MySqlClient&) = delete;
		MySqlClient& operator=(const MySqlClient&) = delete;

	public:
		~MySqlClient();

		// This static method returns a reference to the Singleton instance of 'MySqlClient'
		// based on the provided database configuration in the form of a 'Json::Value'.
		static MySqlClient& GetInstance(const Json::Value& dbconfig) noexcept;

		// This method is used to execute a database query specific to SQLite.
		const Json::Value executeQuery(const std::string& query) const override;
	};
}
}
