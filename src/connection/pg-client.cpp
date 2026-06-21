#include <connection/pg-client.hpp>

using worm::connection::PgClient;

PgClient::PgClient(const std::string& connectionData)
{
	conn = new pqxx::connection(connectionData);
}

PgClient::~PgClient()
{
	delete conn;
}

PgClient& PgClient::GetInstance(const Json::Value& dbconfig) noexcept
{
	std::string connectionData = "host=" + dbconfig["host"].asString()
		+ " port=" + dbconfig["port"].asString()
		+ " dbname=" + dbconfig["dbname"].asString()
		+ " user=" + dbconfig["username"].asString()
		+ " password=" + dbconfig["password"].asString();

	static PgClient instance(connectionData);
	return instance;
}

const Json::Value PgClient::executeQuery(const std::string& query) const
{
	Json::Value results;
	pqxx::work worker = pqxx::work(*conn);
	pqxx::result response = worker.exec(query);
	worker.commit();

	if (isSelect(query)) {
		for (pqxx::result::size_type i = 0; i < response.size(); i++) {
			Json::Value result;

			for (pqxx::result::size_type j = 0; j < response[i].size(); j++) {
				const std::string columnName = response[i][j].name();
				result[columnName] = response[i][j].c_str();
			}

			results["results"].append(result);
		}
	}

	return results;
}
