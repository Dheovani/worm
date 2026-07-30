#include <connection/drivers/mysql-client.hpp>
#include <errors/database-connection-exception.hpp>
#include <errors/query-execution-exception.hpp>
#include <errors/transaction-exception.hpp>

#include <algorithm>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

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

namespace worm::connection
{
  MySqlClient::MySqlClient(const ConnectionConfig& databaseConfig)
    : connection_(mysql_init(nullptr), mysql_close)
  {
    const unsigned int port = static_cast<unsigned int>(std::stoul(databaseConfig.port));

    if (connection_ == nullptr) {
      throw DatabaseConnectionException("Unable to initialize the MySQL client.");
    }

    if (mysql_real_connect(
          connection_.get(),
          databaseConfig.host.c_str(),
          databaseConfig.username.c_str(),
          databaseConfig.password.c_str(),
          databaseConfig.dbname.c_str(),
          port,
          nullptr,
          0) == nullptr) {
      throw DatabaseConnectionException(mysql_error(connection_.get()));
    }
  }

  worm::core::ResultSet MySqlClient::execute(const worm::core::Statement& statement)
  {
    std::vector<core::ResultRow> rows;
    std::vector<MYSQL_FIELD> fields;

    MYSQL_RES* res;
    MYSQL_ROW row;

    if (statement.parameters.empty()) {
      if (mysql_query(connection_.get(), statement.sql.c_str())) {
        throw QueryExecutionException(mysql_error(connection_.get()));
      }

      res = mysql_store_result(connection_.get());

      if (res != nullptr) {
        MYSQL_FIELD* field;
        while ((field = mysql_fetch_field(res))) {
          fields.push_back(*field);
        }

        while ((row = mysql_fetch_row(res))) {
          std::vector<core::ResultColumn> columns;

          for (unsigned int i = 0; i < mysql_num_fields(res); ++i) {
            columns.push_back({fields[i].name, mysqlValue(fields[i], row[i])});
          }

          rows.push_back({columns});
        }

        mysql_free_result(res);
      } else if (mysql_field_count(connection_.get()) != 0) {
        throw QueryExecutionException(mysql_error(connection_.get()));
      }

      const std::uint64_t affectedRows = static_cast<std::uint64_t>(mysql_affected_rows(connection_.get()));

      return core::ResultSet{rows, affectedRows};
    }

    MYSQL_STMT* preparedStatement = mysql_stmt_init(connection_.get());

    if (preparedStatement == nullptr) {
      throw QueryExecutionException("Unable to initialize a MySQL prepared statement.");
    }

    if (mysql_stmt_prepare(preparedStatement, statement.sql.c_str(), static_cast<unsigned long>(statement.sql.size()))) {
      const std::string error = mysql_stmt_error(preparedStatement);
      mysql_stmt_close(preparedStatement);
      throw QueryExecutionException(error);
    }

    std::vector<MySqlBoundParameter> parameters;
    parameters.reserve(statement.parameters.size());

    for (const core::Parameter& parameter : statement.parameters) {
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
      throw QueryExecutionException(error);
    }

    if (mysql_stmt_execute(preparedStatement)) {
      const std::string error = mysql_stmt_error(preparedStatement);
      mysql_stmt_close(preparedStatement);
      throw QueryExecutionException(error);
    }

    res = mysql_stmt_result_metadata(preparedStatement);

    if (res == nullptr) {
      const std::uint64_t affectedRows = static_cast<std::uint64_t>(mysql_stmt_affected_rows(preparedStatement));
      mysql_stmt_close(preparedStatement);
      return core::ResultSet{rows, affectedRows};
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
      throw QueryExecutionException(error);
    }

    while (mysql_stmt_fetch(preparedStatement) == 0) {
      std::vector<core::ResultColumn> columns;

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

    const std::uint64_t affectedRows = static_cast<std::uint64_t>(mysql_stmt_affected_rows(preparedStatement));

    mysql_free_result(res);
    mysql_stmt_close(preparedStatement);
    return core::ResultSet{rows, affectedRows};
  }

  DatabaseType MySqlClient::type() const noexcept
  {
    return DatabaseType::MySQL;
  }

  void MySqlClient::beginTransactionImpl()
  {
    if (transactionActive_) {
      throw worm::TransactionException("A MySQL transaction is already active.");
    }

    if (mysql_query(connection_.get(), "START TRANSACTION")) {
      throw QueryExecutionException(mysql_error(connection_.get()));
    }

    transactionActive_ = true;
  }

  void MySqlClient::commitTransaction()
  {
    if (!transactionActive_) {
      throw worm::TransactionException("There is no active MySQL transaction to commit.");
    }

    if (mysql_query(connection_.get(), "COMMIT")) {
      throw QueryExecutionException(mysql_error(connection_.get()));
    }

    transactionActive_ = false;
  }

  void MySqlClient::rollbackTransaction()
  {
    if (!transactionActive_) {
      throw worm::TransactionException("There is no active MySQL transaction to rollback.");
    }

    if (mysql_query(connection_.get(), "ROLLBACK")) {
      throw QueryExecutionException(mysql_error(connection_.get()));
    }

    transactionActive_ = false;
  }
} // namespace worm::connection
