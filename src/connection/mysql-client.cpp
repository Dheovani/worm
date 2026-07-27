#include <connection/mysql-client.hpp>
#include <errors/database-exception.hpp>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using worm::connection::MySqlClient;

namespace
{
  worm::core::Parameter mysqlValue(const MYSQL_FIELD& field, const char* value)
  {
    if (value == nullptr) {
      return nullptr;
    }

    switch (field.type) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONGLONG:
      return static_cast<std::int64_t>(std::strtoll(value, nullptr, 10));
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL:
      return std::strtod(value, nullptr);
    default:
      return std::string{value};
    }
  }
} // namespace

MySqlClient::MySqlClient(const char* host, const char* user, const char* passwd, const char* db, unsigned int port)
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

MySqlClient& MySqlClient::getInstance(const worm::connection::ConnectionConfig& databaseConfig)
{
  const unsigned int port = static_cast<unsigned int>(std::stoul(databaseConfig.port));

  static MySqlClient instance(
    databaseConfig.host.c_str(),
    databaseConfig.username.c_str(),
    databaseConfig.password.c_str(),
    databaseConfig.dbname.c_str(),
    port);
  return instance;
}

worm::core::ResultSet MySqlClient::executeQuery(const std::string& query) const
{
  std::vector<worm::core::ResultRow> rows;
  std::vector<MYSQL_FIELD> fields;

  MYSQL_RES* res;
  MYSQL_ROW row;

  if (mysql_query(connection_, query.c_str())) {
    std::cerr << "Error executing query: " << mysql_error(connection_) << std::endl;
  } else if (isSelect(query)) {
    res = mysql_store_result(connection_);

    if (res) {
      MYSQL_FIELD* field;
      while ((field = mysql_fetch_field(res))) {
        fields.push_back(*field);
      }

      while ((row = mysql_fetch_row(res))) {
        std::vector<worm::core::ResultColumn> columns;

        for (unsigned int i = 0; i < mysql_num_fields(res); ++i) {
          columns.push_back({fields[i].name, mysqlValue(fields[i], row[i])});
        }

        rows.push_back({columns});
      }

      mysql_free_result(res);
    } else {
      std::cerr << "Error fetching results: " << mysql_error(connection_) << std::endl;
    }
  }

  return worm::core::ResultSet{rows};
}
