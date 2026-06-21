#pragma once

#include <sqlite3.h>
#include <connection/client.hpp>

namespace worm
{
namespace connection
{
	// SQLite-specific database client.
	class SqLiteClient final : public Client
	{
	private:
		enum ErrorHandlingAction
		{
			CLOSE_CONN,
			FINALIZE_STMT
		};

		sqlite3* conn;  // Pointer to the SQLite connection.

		SqLiteClient(const char* dbname);

		// Prevent the use of copy constructor and assignment operator for safety.
		SqLiteClient(const SqLiteClient&) = delete;
		SqLiteClient& operator=(const SqLiteClient&) = delete;

		// Deals with possible errors according to the action defined by the user
		void HandleError(const ErrorHandlingAction& action, sqlite3_stmt* stmt = nullptr) const;

	public:
		~SqLiteClient();

		// This static method returns a reference to the Singleton instance of 'SqLiteClient'
		// based on the provided database configuration in the form of a 'Json::Value'.
		static SqLiteClient& GetInstance(const Json::Value& dbconfig) noexcept;

		// This method is used to execute a database query specific to SQLite.
		const Json::Value executeQuery(const std::string& query) const override;
	};
}
}
