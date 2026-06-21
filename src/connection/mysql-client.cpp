#include <connection/mysql-client.hpp>
#include <errors/database-exception.hpp>

using worm::connection::MySqlClient;

MySqlClient::MySqlClient(
	const char* host,
	const char* user,
	const char* passwd,
	const char* db,
	unsigned int port
) {
	conn = mysql_init(NULL);

	if (conn == NULL) {
		throw worm::DatabaseException(mysql_error(conn));
	}

	if (mysql_real_connect(conn, host, user, passwd, db, port, NULL, 0) == NULL) {
		const char* msg = mysql_error(conn);
		mysql_close(conn);
		throw worm::DatabaseException(msg);
	}
}

MySqlClient::~MySqlClient()
{
	mysql_close(conn);
}

MySqlClient& MySqlClient::GetInstance(const Json::Value& dbconfig) noexcept
{
	const std::string host = dbconfig["host"].asString();
	const std::string username = dbconfig["username"].asString();
	const std::string password = dbconfig["password"].asString();
	const std::string dbname = dbconfig["dbname"].asString();
	const unsigned int port = dbconfig["port"].asUInt();

	static MySqlClient instance(host.c_str(), username.c_str(), password.c_str(), dbname.c_str(), port);
	return instance;
}

const Json::Value MySqlClient::executeQuery(const std::string& query) const
{
	Json::Value results;
	std::vector<char*> columns;

	MYSQL_RES* res;
	MYSQL_ROW row;

	if (mysql_query(conn, query.c_str())) {
		std::cerr << "Error executing query: " << mysql_error(conn) << std::endl;
	}
	else if (isSelect(query)) {
		res = mysql_store_result(conn);

		if (res) {
			MYSQL_FIELD* field;
			while ((field = mysql_fetch_field(res))) {
				columns.push_back(field->name);
			}

			while ((row = mysql_fetch_row(res))) {
				Json::Value result;

				for (unsigned int i = 0; i < mysql_num_fields(res); ++i) {
					result[columns[i]] = row[i] ? row[i] : "NULL";
				}

				results["results"].append(result);
			}

			mysql_free_result(res);
		}
		else {
			std::cerr << "Error fetching results: " << mysql_error(conn) << std::endl;
		}
	}

	return results;
}
