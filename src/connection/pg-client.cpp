#include <connection/pg-client.hpp>

using worm::connection::PgClient;

PgClient::PgClient(const std::string& connectionData)
{
  connection_ = new pqxx::connection(connectionData);
}

PgClient::~PgClient()
{
  delete connection_;
}

PgClient& PgClient::getInstance(const Json::Value& databaseConfig)
{
  std::string connectionData = "host=" + databaseConfig["host"].asString() +
                               " port=" + databaseConfig["port"].asString() +
                               " dbname=" + databaseConfig["dbname"].asString() +
                               " user=" + databaseConfig["username"].asString() +
                               " password=" + databaseConfig["password"].asString();

  static PgClient instance(connectionData);
  return instance;
}

Json::Value PgClient::executeQuery(const std::string& query) const
{
  Json::Value results;
  pqxx::work worker = pqxx::work(*connection_);
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
