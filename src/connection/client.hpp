#pragma once

#include <map>
#include <sstream>
#include <iostream>
#include <json/json.h>

namespace worm
{
	/**
	 * Supported database types
	 * Whenever you need to specify a database type in your code,
	 * you should use one of the values from this enum
	 * instead of using strings or magic numbers.
	 */
	enum DbTypes
	{
		PostgreSQL, // Requires pqxx lib
		MySQL,      // Requires mysql lib
		SQLite      // Requires sqlite3 lib
	};

	/**
	 * The 'types' map is used to identify the user's database type
	 * based on keys provided in a 'dbconfig.json' file.
	 * 
	 * An example of the 'dbconfig.json' file:
	 * {
	 *         "database": "postgresql",
	 *         "host": "localhost",
	 *         "port": 5432,
	 *         "dbname": "mydb",
	 *         "username": "myuser",
	 *         "password": "mypassword"
	 * }
	 * 
	 * Each key in the map corresponds to a supported database type
	 * and maps to the appropriate enum value from the 'DbTypes' enum.
	 */
	const std::map<std::string, DbTypes> types = {
		{ "postgresql", PostgreSQL },
		{ "mysql", MySQL },
		{ "sqlite", SQLite }
	};

	namespace connection
	{
		/**
		 * The 'Client' class serves as a Singleton interface to interact with specific
		 * database classes. It provides a unified way to access and interact with different
		 * database types, based on the 'DbTypes' enumeration.
		 */
		class Client
		{
		protected:
			Client() = default;

			bool isSelect(const std::string& query) const;

		private:
			// Prevent the use of copy constructor and assignment operator for safety.
			Client(const Client&) = delete;
			Client& operator=(const Client&) = delete;

		public:
			// This pure virtual method, 'executeQuery', is used to execute database queries.
			// It must be implemented by specific database client classes derived from 'Client'.
			virtual const Json::Value executeQuery(const std::string& query) const = 0;
		};
	}
}
