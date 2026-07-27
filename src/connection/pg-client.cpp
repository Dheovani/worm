#include <connection/pg-client.hpp>

#include <vector>

using worm::connection::PgClient;

PgClient::PgClient(const std::string& connectionData)
{
  connection_ = new pqxx::connection(connectionData);
}

PgClient::~PgClient()
{
  delete connection_;
}

PgClient& PgClient::getInstance(const worm::connection::ConnectionConfig& databaseConfig)
{
  std::string connectionData =
    " host=" + databaseConfig.host +
    " port=" + databaseConfig.port +
    " dbname=" + databaseConfig.dbname +
    " user=" + databaseConfig.username +
    " password=" + databaseConfig.password;

  static PgClient instance(connectionData);
  return instance;
}

worm::core::ResultSet PgClient::executeQuery(const std::string& query) const
{
  std::vector<worm::core::ResultRow> rows;
  pqxx::work worker = pqxx::work(*connection_);
  pqxx::result response = worker.exec(query);
  worker.commit();

  if (isSelect(query)) {
    for (pqxx::result::size_type i = 0; i < response.size(); i++) {
      std::vector<worm::core::ResultColumn> columns;

      for (pqxx::result::size_type j = 0; j < response[i].size(); j++) {
        const std::string columnName = response[i][j].name();
        worm::core::Parameter columnValue = nullptr;

        if (!response[i][j].is_null()) {
          columnValue = response[i][j].c_str();
        }

        columns.push_back({columnName, columnValue});
      }

      rows.push_back({columns});
    }
  }

  return worm::core::ResultSet{rows};
}
