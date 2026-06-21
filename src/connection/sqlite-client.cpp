#include <connection/sqlite-client.hpp>
#include <errors/database-exception.hpp>

using worm::connection::SqLiteClient;

SqLiteClient::SqLiteClient(const char* dbname)
{
	int rc = sqlite3_open(dbname, &conn);

	// Error connecting to DB
	if (rc != SQLITE_OK)
		HandleError(CLOSE_CONN);
}

SqLiteClient::~SqLiteClient()
{
	sqlite3_close(conn);
}

void SqLiteClient::HandleError(const ErrorHandlingAction& action, sqlite3_stmt* stmt) const
{
	try {
		const char* msg = sqlite3_errmsg(conn);

		if (action == CLOSE_CONN) {
			sqlite3_close(conn);
			throw worm::DatabaseException(msg);
		}
		else if (action == FINALIZE_STMT && stmt) {
			sqlite3_finalize(stmt);
			std::cerr << msg << std::endl;
		}
	}
	catch (const std::exception& e) {
		sqlite3_close(conn);
		throw worm::DatabaseException(e.what());
	}
}

SqLiteClient& SqLiteClient::GetInstance(const Json::Value& dbconfig) noexcept
{
	const std::string& dbname = dbconfig["dbname"].asString();
	static SqLiteClient instance(dbname.c_str());
	return instance;
}

const Json::Value SqLiteClient::executeQuery(const std::string& query) const
{
	Json::Value results;
	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(conn, query.c_str(), static_cast<int>(query.size()), &stmt, nullptr);

	if (rc != SQLITE_OK) {
		std::cerr << sqlite3_errmsg(conn) << std::endl;

		if (stmt)
			HandleError(FINALIZE_STMT, stmt);

		return Json::Value();
	}

	if (isSelect(query)) {
		int columnCount = sqlite3_column_count(stmt);

		while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
			Json::Value result;

			for (int i = 0; i < columnCount; i++) {
				const char* columnName = reinterpret_cast<const char*>(sqlite3_column_name(stmt, i));
				const char* columnValue = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));

				if (columnValue == NULL)
					columnValue = "";

				result[columnName] = columnValue;
			}

			results["results"].append(result);
		}
	}

	if (rc != SQLITE_DONE)
		std::cerr << sqlite3_errmsg(conn) << std::endl;

	sqlite3_finalize(stmt);
	return results;
}
