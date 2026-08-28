#include <connection/drivers/mysql-client.hpp>
#include <errors/database-connection-exception.hpp>
#include <errors/invalid-arg-exception.hpp>
#include <errors/query-execution-exception.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace
{
  constexpr unsigned long resultBufferSize = 4096;

  enum class MySqlParameterKind
  {
    Null,
    Integer,
    Floating,
    Text,
    Decimal,
    Binary
  };

  struct MySqlBoundParameter
  {
    MYSQL_BIND bind{};
    MySqlParameterKind kind{MySqlParameterKind::Null};
    std::int64_t integer{};
    double floating{};
    std::string text;
    std::vector<std::byte> binary;
    unsigned long length{};
    bool isNull{};
  };

  struct MySqlBoolFlag
  {
    bool value{};
  };

  void refreshMySqlBind(MySqlBoundParameter& boundParameter)
  {
    std::memset(&boundParameter.bind, 0, sizeof(boundParameter.bind));

    switch (boundParameter.kind) {
    case MySqlParameterKind::Null:
      boundParameter.bind.buffer_type = MYSQL_TYPE_NULL;
      boundParameter.bind.is_null = &boundParameter.isNull;
      return;
    case MySqlParameterKind::Integer:
      boundParameter.bind.buffer_type = MYSQL_TYPE_LONGLONG;
      boundParameter.bind.buffer = &boundParameter.integer;
      return;
    case MySqlParameterKind::Floating:
      boundParameter.bind.buffer_type = MYSQL_TYPE_DOUBLE;
      boundParameter.bind.buffer = &boundParameter.floating;
      return;
    case MySqlParameterKind::Text:
      boundParameter.bind.buffer_type = MYSQL_TYPE_STRING;
      boundParameter.bind.buffer = boundParameter.text.data();
      boundParameter.bind.buffer_length = boundParameter.length;
      boundParameter.bind.length = &boundParameter.length;
      return;
    case MySqlParameterKind::Decimal:
      boundParameter.bind.buffer_type = MYSQL_TYPE_NEWDECIMAL;
      boundParameter.bind.buffer = boundParameter.text.data();
      boundParameter.bind.buffer_length = boundParameter.length;
      boundParameter.bind.length = &boundParameter.length;
      return;
    case MySqlParameterKind::Binary:
      boundParameter.bind.buffer_type = MYSQL_TYPE_BLOB;
      boundParameter.bind.buffer = boundParameter.binary.data();
      boundParameter.bind.buffer_length = boundParameter.length;
      boundParameter.bind.length = &boundParameter.length;
      return;
    }
  }

  bool isBinaryField(const MYSQL_FIELD& field) noexcept
  {
    return field.charsetnr == 63;
  }

  worm::core::Parameter mysqlValue(const MYSQL_FIELD& field, const char* value, unsigned long length)
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
      return std::strtod(value, nullptr);
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL:
      return worm::core::Decimal{std::string{value, length}};
    case MYSQL_TYPE_BIT:
      if (field.length == 1) {
        return length != 0 && value[0] != 0;
      }
      return worm::core::Binary{
        std::span<const std::byte>{reinterpret_cast<const std::byte*>(value), static_cast<std::size_t>(length)}};
    default:
      if (isBinaryField(field)) {
        return worm::core::Binary{
          std::span<const std::byte>{reinterpret_cast<const std::byte*>(value), static_cast<std::size_t>(length)}};
      }
      return std::string{value, length};
    }
  }

  MySqlBoundParameter mysqlParameter(const worm::core::Parameter& parameter)
  {
    MySqlBoundParameter boundParameter;

    std::visit(
      [&boundParameter](const auto& value) {
        using Value = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<Value, std::nullptr_t>) {
          boundParameter.isNull = true;
          boundParameter.kind = MySqlParameterKind::Null;
        } else if constexpr (std::is_same_v<Value, std::int64_t>) {
          boundParameter.kind = MySqlParameterKind::Integer;
          boundParameter.integer = value;
        } else if constexpr (std::is_same_v<Value, double>) {
          boundParameter.kind = MySqlParameterKind::Floating;
          boundParameter.floating = value;
        } else if constexpr (std::is_same_v<Value, bool>) {
          boundParameter.kind = MySqlParameterKind::Integer;
          boundParameter.integer = value ? 1 : 0;
        } else if constexpr (std::is_same_v<Value, worm::core::Decimal>) {
          boundParameter.kind = MySqlParameterKind::Decimal;
          boundParameter.text = value.value();
          boundParameter.length = static_cast<unsigned long>(boundParameter.text.size());
        } else if constexpr (std::is_same_v<Value, worm::core::Binary>) {
          boundParameter.kind = MySqlParameterKind::Binary;
          boundParameter.binary.assign(value.value().begin(), value.value().end());
          boundParameter.length = static_cast<unsigned long>(boundParameter.binary.size());
        } else {
          boundParameter.kind = MySqlParameterKind::Text;
          boundParameter.text = value;
          boundParameter.length = static_cast<unsigned long>(boundParameter.text.size());
        }
      },
      parameter);

    refreshMySqlBind(boundParameter);

    return boundParameter;
  }

  unsigned int mysqlTimeoutSeconds(std::chrono::milliseconds timeout)
  {
    const std::chrono::seconds seconds = worm::connection::timeoutSeconds(timeout);
    if (seconds.count() > (std::numeric_limits<unsigned int>::max)()) {
      throw worm::InvalidArgException("MySQL timeout is too large.");
    }

    return static_cast<unsigned int>(seconds.count());
  }
} // namespace

namespace worm::connection
{
  MySqlClient::MySqlClient(const ConnectionConfig& databaseConfig)
    : Client(databaseConfig.cacheResults),
      connection_(mysql_init(nullptr), mysql_close)
  {
    const unsigned int port = static_cast<unsigned int>(std::stoul(databaseConfig.port));

    if (connection_ == nullptr) {
      throw DatabaseConnectionException("Unable to initialize the MySQL client.");
    }

    if (databaseConfig.timeoutConfig.connectionTimeout.has_value()) {
      unsigned int timeout = mysqlTimeoutSeconds(*databaseConfig.timeoutConfig.connectionTimeout);
      if (mysql_options(connection_.get(), MYSQL_OPT_CONNECT_TIMEOUT, &timeout) != 0) {
        throw DatabaseConnectionException(mysql_error(connection_.get()));
      }
    }

    if (databaseConfig.timeoutConfig.queryTimeout.has_value()) {
      unsigned int timeout = mysqlTimeoutSeconds(*databaseConfig.timeoutConfig.queryTimeout);
      if (mysql_options(connection_.get(), MYSQL_OPT_READ_TIMEOUT, &timeout) != 0 ||
          mysql_options(connection_.get(), MYSQL_OPT_WRITE_TIMEOUT, &timeout) != 0) {
        throw DatabaseConnectionException(mysql_error(connection_.get()));
      }
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

  worm::core::ResultSet MySqlClient::executeImpl(const worm::core::Statement& statement)
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
          const unsigned long* lengths = mysql_fetch_lengths(res);

          for (unsigned int i = 0; i < mysql_num_fields(res); ++i) {
            columns.push_back({fields[i].name, mysqlValue(fields[i], row[i], lengths[i])});
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

    if (mysql_stmt_prepare(
          preparedStatement,
          statement.sql.c_str(),
          static_cast<unsigned long>(statement.sql.size()))) {
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
      refreshMySqlBind(parameter);
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

    int fetchResult = 0;
    while ((fetchResult = mysql_stmt_fetch(preparedStatement)) == 0 || fetchResult == MYSQL_DATA_TRUNCATED) {
      std::vector<core::ResultColumn> columns;

      for (unsigned int i = 0; i < columnCount; i++) {
        if (isNull[i].value) {
          columns.push_back({fields[i].name, nullptr});
          continue;
        }

        const char* value = buffers[i].data();
        std::string expanded;
        if (errors[i].value) {
          expanded.resize(lengths[i]);
          MYSQL_BIND expandedBind{};
          expandedBind.buffer_type = MYSQL_TYPE_STRING;
          expandedBind.buffer = expanded.data();
          expandedBind.buffer_length = lengths[i];
          expandedBind.length = &lengths[i];
          if (mysql_stmt_fetch_column(preparedStatement, &expandedBind, i, 0)) {
            const std::string error = mysql_stmt_error(preparedStatement);
            mysql_free_result(res);
            mysql_stmt_close(preparedStatement);
            throw QueryExecutionException(error);
          }
          value = expanded.data();
        }
        columns.push_back({fields[i].name, mysqlValue(fields[i], value, lengths[i])});
      }

      rows.push_back({columns});
    }

    if (fetchResult != MYSQL_NO_DATA) {
      const std::string error = mysql_stmt_error(preparedStatement);
      mysql_free_result(res);
      mysql_stmt_close(preparedStatement);
      throw QueryExecutionException(error);
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
    if (mysql_query(connection_.get(), "START TRANSACTION")) {
      throw QueryExecutionException(mysql_error(connection_.get()));
    }
  }

  void MySqlClient::commitTransactionImpl()
  {
    if (mysql_query(connection_.get(), "COMMIT")) {
      throw QueryExecutionException(mysql_error(connection_.get()));
    }
  }

  void MySqlClient::rollbackTransactionImpl()
  {
    if (mysql_query(connection_.get(), "ROLLBACK")) {
      throw QueryExecutionException(mysql_error(connection_.get()));
    }
  }
} // namespace worm::connection
