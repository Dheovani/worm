#include <connection/mysql-client.hpp>
#include <errors/database-exception.hpp>

#include <algorithm>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

using worm::connection::MySqlClient;

namespace
{
  constexpr unsigned long resultBufferSize = 4096;

  struct MySqlBoundParameter
  {
    MYSQL_BIND bind{};
    std::int64_t integer{};
    double floating{};
    std::string text;
    unsigned long length{};
    bool isNull{};
  };

  struct MySqlBoolFlag
  {
    bool value{};
  };

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

  MySqlBoundParameter mysqlParameter(const worm::core::Parameter& parameter)
  {
    MySqlBoundParameter boundParameter;
    std::memset(&boundParameter.bind, 0, sizeof(boundParameter.bind));

    std::visit(
      [&boundParameter](const auto& value)
      {
        using Value = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<Value, std::nullptr_t>) {
          boundParameter.isNull = true;
          boundParameter.bind.buffer_type = MYSQL_TYPE_NULL;
          boundParameter.bind.is_null = &boundParameter.isNull;
        } else if constexpr (std::is_same_v<Value, std::int64_t>) {
          boundParameter.integer = value;
          boundParameter.bind.buffer_type = MYSQL_TYPE_LONGLONG;
          boundParameter.bind.buffer = &boundParameter.integer;
        } else if constexpr (std::is_same_v<Value, double>) {
          boundParameter.floating = value;
          boundParameter.bind.buffer_type = MYSQL_TYPE_DOUBLE;
          boundParameter.bind.buffer = &boundParameter.floating;
        } else if constexpr (std::is_same_v<Value, bool>) {
          boundParameter.integer = value ? 1 : 0;
          boundParameter.bind.buffer_type = MYSQL_TYPE_LONGLONG;
          boundParameter.bind.buffer = &boundParameter.integer;
        } else {
          boundParameter.text = value;
          boundParameter.length = static_cast<unsigned long>(boundParameter.text.size());
          boundParameter.bind.buffer_type = MYSQL_TYPE_STRING;
          boundParameter.bind.buffer = boundParameter.text.data();
          boundParameter.bind.buffer_length = boundParameter.length;
          boundParameter.bind.length = &boundParameter.length;
        }
      },
      parameter);

    return boundParameter;
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

worm::core::ResultSet MySqlClient::executeQuery(const worm::core::Statement& statement) const
{
  std::vector<worm::core::ResultRow> rows;
  std::vector<MYSQL_FIELD> fields;

  MYSQL_RES* res;
  MYSQL_ROW row;

  if (statement.parameters.empty()) {
    if (mysql_query(connection_, statement.sql.c_str())) {
      std::cerr << "Error executing query: " << mysql_error(connection_) << std::endl;
    } else if (isSelect(statement.sql)) {
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

  MYSQL_STMT* preparedStatement = mysql_stmt_init(connection_);

  if (preparedStatement == nullptr) {
    throw worm::DatabaseException("Unable to initialize a MySQL prepared statement.");
  }

  if (mysql_stmt_prepare(preparedStatement, statement.sql.c_str(), static_cast<unsigned long>(statement.sql.size()))) {
    const std::string error = mysql_stmt_error(preparedStatement);
    mysql_stmt_close(preparedStatement);
    throw worm::DatabaseException(error);
  }

  std::vector<MySqlBoundParameter> parameters;
  parameters.reserve(statement.parameters.size());

  for (const worm::core::Parameter& parameter : statement.parameters) {
    parameters.push_back(mysqlParameter(parameter));
  }

  std::vector<MYSQL_BIND> binds;
  binds.reserve(parameters.size());

  for (MySqlBoundParameter& parameter : parameters) {
    binds.push_back(parameter.bind);
  }

  if (!binds.empty() && mysql_stmt_bind_param(preparedStatement, binds.data())) {
    const std::string error = mysql_stmt_error(preparedStatement);
    mysql_stmt_close(preparedStatement);
    throw worm::DatabaseException(error);
  }

  if (mysql_stmt_execute(preparedStatement)) {
    const std::string error = mysql_stmt_error(preparedStatement);
    mysql_stmt_close(preparedStatement);
    throw worm::DatabaseException(error);
  }

  if (!isSelect(statement.sql)) {
    mysql_stmt_close(preparedStatement);
    return worm::core::ResultSet{rows};
  }

  res = mysql_stmt_result_metadata(preparedStatement);

  if (res == nullptr) {
    mysql_stmt_close(preparedStatement);
    return worm::core::ResultSet{rows};
  }

  MYSQL_FIELD* field;
  while ((field = mysql_fetch_field(res))) {
    fields.push_back(*field);
  }

  const unsigned int columnCount = mysql_num_fields(res);
  std::vector<std::string> buffers(columnCount, std::string(resultBufferSize, '\0'));
  std::vector<unsigned long> lengths(columnCount);
  std::vector<MySqlBoolFlag> isNull(columnCount);
  std::vector<MySqlBoolFlag> errors(columnCount);
  std::vector<MYSQL_BIND> resultBinds(columnCount);

  for (unsigned int i = 0; i < columnCount; i++) {
    std::memset(&resultBinds[i], 0, sizeof(resultBinds[i]));
    resultBinds[i].buffer_type = MYSQL_TYPE_STRING;
    resultBinds[i].buffer = buffers[i].data();
    resultBinds[i].buffer_length = resultBufferSize;
    resultBinds[i].length = &lengths[i];
    resultBinds[i].is_null = &isNull[i].value;
    resultBinds[i].error = &errors[i].value;
  }

  if (mysql_stmt_bind_result(preparedStatement, resultBinds.data())) {
    const std::string error = mysql_stmt_error(preparedStatement);
    mysql_free_result(res);
    mysql_stmt_close(preparedStatement);
    throw worm::DatabaseException(error);
  }

  while (mysql_stmt_fetch(preparedStatement) == 0) {
    std::vector<worm::core::ResultColumn> columns;

    for (unsigned int i = 0; i < columnCount; i++) {
      if (isNull[i].value) {
        columns.push_back({fields[i].name, nullptr});
        continue;
      }

      buffers[i][(std::min)(lengths[i], resultBufferSize - 1)] = '\0';
      columns.push_back({fields[i].name, mysqlValue(fields[i], buffers[i].c_str())});
    }

    rows.push_back({columns});
  }

  mysql_free_result(res);
  mysql_stmt_close(preparedStatement);
  return worm::core::ResultSet{rows};
}
