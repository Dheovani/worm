#pragma once

#include <cstdlib>
#include <fstream>
#include <json/json.h>
#include <utils/logger.hpp>
#include <utils/helpers.hpp>
#include <utils/client-factory.hpp>
#include <errors/database-exception.hpp>

using worm::Logger;

#define UseDependencyInjection \
	template <class _Ty> auto GetDI() noexcept { return worm::DependencyInjector<_Ty>(); }

namespace worm
{
	template <class _Ty>
	class DependencyInjector
	{
	public:
		_Ty Get() const
		{
			return _Ty();
		}
	};

	/** Specialization class for Logger dependency */
	template <>
	class DependencyInjector<Logger>
	{
#if !defined(_DFLT_LVL_INFO) && !defined(_DFLT_LVL_DEBUG) && !defined(_DFLT_LVL_TRACE) && !defined(_DFLT_LVL_WARNING)
#define _DFLT_LVL_DEBUG
#endif // !_DFLT_LVL_DEBUG
	public:
		template <class _Class, size_t _Idx>
		Logger Get() const
		{
			return Logger(typeid(_Class).name(), _Idx);
		}
	};

	/** Specialization class for database client dependency */
	template <>
	class DependencyInjector<connection::Client>
	{
	public:
		connection::Client& Get() const
		{
			const std::string db = utils::env::GetDatabaseType();
			Json::Value dbconfig;

			char
				*host = NULL,
				*username = NULL,
				*password = NULL,
				*dbname = NULL,
				*port = NULL;

			if (host = std::getenv("host"))
				dbconfig["host"] = host;
			if (username = std::getenv("username"))
				dbconfig["username"] = username;
			if (password = std::getenv("password"))
				dbconfig["password"] = password;
			if (dbname = std::getenv("dbname"))
				dbconfig["dbname"] = dbname;
			if (port = std::getenv("port"))
				dbconfig["port"] = port;

			return worm::GetInstance(dbconfig, worm::types.at(db));
		}
	};

	/** Specialization that returns the DbTypes value */
	template <>
	class DependencyInjector<DbTypes>
	{
	public:
		const DbTypes& Get() const
		{
			const std::string db = utils::env::GetDatabaseType();

			if (db.empty()) 
				throw worm::DatabaseException("Unsupported Database type.");

			return worm::types.at(db);
		}
	};
}
