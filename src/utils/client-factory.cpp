#include <utils/client-factory.hpp>
#include <connection/mysql-client.hpp>
#include <connection/pgclient.hpp>
#include <connection/sqlite-client.hpp>
#include <errors/database-exception.hpp>

worm::connection::Client& worm::GetInstance(const Json::Value& connectionData, const worm::DbTypes& type)
{
	switch (type) {
	case worm::DbTypes::PostgreSQL:
		return worm::connection::PgClient::GetInstance(connectionData);

	case worm::DbTypes::MySQL:
		return worm::connection::MySqlClient::GetInstance(connectionData);

	case worm::DbTypes::SQLite:
		return worm::connection::SqLiteClient::GetInstance(connectionData);

	default:
		throw worm::DatabaseException("Unsupported Database type.");
	}
}
