#include <connection/mysql-client.hpp>
#include <errors/database-exception.hpp>

#include <iostream>
#include <string>
#include <vector>

using worm::connection::MySqlClient;

MySqlClient::MySqlClient(const char* host, const char* user, const char* passwd, const char* db,
                         unsigned int port)
{
  connection_ = mysql_init(nullptr);

  if (connection_ == nullptr) {
    throw worm::DatabaseException("Unable to initialize the MySQL client.");
  }

  if (mysql_real_connect(connection_, host, user, passwd, db, port, nullptr, 0) == nullptr) {
    const char* msg = mysql_error(connection_);
    mysql_close(connection_);
    throw worm::DatabaseException(msg);
  }
}

MySqlClient::~MySqlClient()
{
  mysql_close(connection_);
}

MySqlClient& MySqlClient::getInstance(const Json::Value& databaseConfig)
{
  const std::string host = databaseConfig["host"].asString();
  const std::string username = databaseConfig["username"].asString();
  const std::string password = databaseConfig["password"].asString();
  const std::string databaseName = databaseConfig["dbname"].asString();
  const unsigned int port = databaseConfig["port"].asUInt();

  static MySqlClient instance(host.c_str(), username.c_str(), password.c_str(),
                              databaseName.c_str(), port);
  return instance;
}

Json::Value MySqlClient::executeQuery(const std::string& query) const
{
  Json::Value results;
  std::vector<char*> columns;

  MYSQL_RES* res;
  MYSQL_ROW row;

  if (mysql_query(connection_, query.c_str())) {
    std::cerr << "Error executing query: " << mysql_error(connection_) << std::endl;
  } else if (isSelect(query)) {
    res = mysql_store_result(connection_);

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
    } else {
      std::cerr << "Error fetching results: " << mysql_error(connection_) << std::endl;
    }
  }

  return results;
}
