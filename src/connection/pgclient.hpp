#pragma once

#include <pqxx/pqxx>
#include <json/json.h>
#include <connection/client.hpp>

namespace worm
{
namespace connection
{
	// PostgreSQL-specific database client.
	class PgClient final : public Client
	{
	private:
		pqxx::connection* conn; // Pointer to the PostgreSQL connection.

		PgClient(const std::string& data);

		// Prevent the use of copy constructor and assignment operator for safety.
		PgClient(const PgClient&) = delete;
		PgClient& operator=(const PgClient&) = delete;

	public:
		~PgClient();

		// This static method returns a reference to the Singleton instance of 'PgClient'
		// based on the provided connection data for PostgreSQL.
		static PgClient& GetInstance(const Json::Value& connectionData) noexcept;

		// This method is used to execute a database query specific to PostgreSQL.
		const Json::Value executeQuery(const std::string& query) const override;
	};
}
}
