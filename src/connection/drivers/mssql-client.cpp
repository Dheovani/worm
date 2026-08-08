#include <connection/drivers/mssql-client.hpp>

#include <errors/database-connection-exception.hpp>
#include <errors/query-execution-exception.hpp>
#include <utils/helpers.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace
{
  constexpr std::size_t diagnosticBufferSize = 1024;
  constexpr std::size_t valueBufferSize = 4096;

  struct BoundParameter
  {
    std::int64_t integer{};
    double floating{};
    SQLCHAR boolean{};
    std::string text;
    SQLLEN indicator{};
    SQLSMALLINT valueType{SQL_C_CHAR};
    SQLSMALLINT parameterType{SQL_VARCHAR};
    SQLULEN columnSize{1};
    SQLPOINTER value{};
    SQLLEN bufferLength{};
  };

  struct StatementDeleter
  {
    using pointer = SQLHSTMT;

    void operator()(SQLHSTMT statement) const noexcept
    {
      if (statement != SQL_NULL_HSTMT) {
        SQLFreeHandle(SQL_HANDLE_STMT, statement);
      }
    }
  };

  std::string diagnostics(SQLSMALLINT handleType, SQLHANDLE handle)
  {
    std::string message;
    std::array<SQLCHAR, 6> state{};
    std::array<SQLCHAR, diagnosticBufferSize> text{};
    SQLINTEGER nativeError = 0;
    SQLSMALLINT length = 0;

    for (SQLSMALLINT record = 1;; ++record) {
      const SQLRETURN result =
        SQLGetDiagRec(handleType, handle, record, state.data(), &nativeError, text.data(), text.size(), &length);

      if (result == SQL_NO_DATA) {
        break;
      }

      if (!SQL_SUCCEEDED(result)) {
        break;
      }

      if (!message.empty()) {
        message += " | ";
      }

      message += reinterpret_cast<const char*>(state.data());
      message += ": ";
      const std::size_t messageLength = (std::min)(static_cast<std::size_t>(length), text.size() - std::size_t{1});
      message.append(reinterpret_cast<const char*>(text.data()), messageLength);
    }

    return message.empty() ? "ODBC operation failed without diagnostics." : message;
  }

  std::string quoteOdbcValue(std::string_view value)
  {
    std::string quoted{"{"};
    quoted.reserve(value.size() + 2);

    for (const char character : value) {
      quoted += character;
      if (character == '}') {
        quoted += '}';
      }
    }

    quoted += '}';
    return quoted;
  }

  std::string buildConnectionString(const worm::connection::ConnectionConfig& config)
  {
    using worm::utils::strings::replaceFirst;

    std::string connectionString = "Driver={driver};Server={server};Database={dbname};UID={username};PWD={password};"
                                   "Encrypt=yes;TrustServerCertificate=no;";

    std::string driver = worm::utils::env::envValue("MSSQL_ODBC_DRIVER");
    if (driver.empty()) {
      driver = "ODBC Driver 18 for SQL Server";
    }

    std::string server = "tcp:" + config.host;
    if (!config.port.empty()) {
      server += "," + config.port;
    }

    replaceFirst(connectionString, "{driver}", quoteOdbcValue(driver));
    replaceFirst(connectionString, "{server}", quoteOdbcValue(server));
    replaceFirst(connectionString, "{dbname}", quoteOdbcValue(config.dbname));
    replaceFirst(connectionString, "{username}", quoteOdbcValue(config.username));
    replaceFirst(connectionString, "{password}", quoteOdbcValue(config.password));

    return connectionString;
  }

  BoundParameter makeParameter(const worm::core::Parameter& parameter)
  {
    BoundParameter bound;

    std::visit(
      [&bound](const auto& value) {
        using Value = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<Value, std::nullptr_t>) {
          bound.indicator = SQL_NULL_DATA;
        } else if constexpr (std::is_same_v<Value, std::int64_t>) {
          bound.integer = value;
          bound.valueType = SQL_C_SBIGINT;
          bound.parameterType = SQL_BIGINT;
          bound.value = &bound.integer;
          bound.bufferLength = sizeof(bound.integer);
        } else if constexpr (std::is_same_v<Value, double>) {
          bound.floating = value;
          bound.valueType = SQL_C_DOUBLE;
          bound.parameterType = SQL_DOUBLE;
          bound.value = &bound.floating;
          bound.bufferLength = sizeof(bound.floating);
        } else if constexpr (std::is_same_v<Value, bool>) {
          bound.boolean = value ? 1 : 0;
          bound.valueType = SQL_C_BIT;
          bound.parameterType = SQL_BIT;
          bound.value = &bound.boolean;
          bound.bufferLength = sizeof(bound.boolean);
        } else {
          bound.text = value;
          bound.valueType = SQL_C_CHAR;
          bound.parameterType = SQL_VARCHAR;
          bound.columnSize = (std::max)(SQLULEN{1}, static_cast<SQLULEN>(bound.text.size()));
          bound.indicator = static_cast<SQLLEN>(bound.text.size());
          bound.value = bound.text.data();
          bound.bufferLength = static_cast<SQLLEN>(bound.text.size());
        }
      },
      parameter);

    return bound;
  }

  void refreshParameterPointers(std::vector<BoundParameter>& parameters)
  {
    for (BoundParameter& parameter : parameters) {
      switch (parameter.valueType) {
      case SQL_C_SBIGINT:
        parameter.value = &parameter.integer;
        break;
      case SQL_C_DOUBLE:
        parameter.value = &parameter.floating;
        break;
      case SQL_C_BIT:
        parameter.value = &parameter.boolean;
        break;
      default:
        parameter.value = parameter.indicator == SQL_NULL_DATA ? nullptr : parameter.text.data();
        break;
      }
    }
  }

  worm::core::Parameter textColumn(SQLHSTMT statement, SQLUSMALLINT index)
  {
    std::string value;

    for (;;) {
      std::array<char, valueBufferSize> buffer{};
      SQLLEN indicator = 0;
      const SQLRETURN result = SQLGetData(statement, index, SQL_C_CHAR, buffer.data(), buffer.size(), &indicator);

      if (indicator == SQL_NULL_DATA) {
        return nullptr;
      }

      if (!SQL_SUCCEEDED(result)) {
        throw worm::QueryExecutionException(diagnostics(SQL_HANDLE_STMT, statement));
      }

      value.append(buffer.data(), std::strlen(buffer.data()));
      if (result == SQL_SUCCESS) {
        return value;
      }
    }
  }

  worm::core::Parameter columnValue(SQLHSTMT statement, SQLUSMALLINT index, SQLSMALLINT dataType)
  {
    SQLLEN indicator = 0;

    switch (dataType) {
    case SQL_TINYINT:
    case SQL_SMALLINT:
    case SQL_INTEGER:
    case SQL_BIGINT: {
      std::int64_t value = 0;
      const SQLRETURN result = SQLGetData(statement, index, SQL_C_SBIGINT, &value, sizeof(value), &indicator);
      if (indicator == SQL_NULL_DATA) {
        return nullptr;
      }
      if (!SQL_SUCCEEDED(result)) {
        throw worm::QueryExecutionException(diagnostics(SQL_HANDLE_STMT, statement));
      }
      return value;
    }
    case SQL_REAL:
    case SQL_FLOAT:
    case SQL_DOUBLE:
    case SQL_DECIMAL:
    case SQL_NUMERIC: {
      double value = 0;
      const SQLRETURN result = SQLGetData(statement, index, SQL_C_DOUBLE, &value, sizeof(value), &indicator);
      if (indicator == SQL_NULL_DATA) {
        return nullptr;
      }
      if (!SQL_SUCCEEDED(result)) {
        throw worm::QueryExecutionException(diagnostics(SQL_HANDLE_STMT, statement));
      }
      return value;
    }
    case SQL_BIT: {
      SQLCHAR value = 0;
      const SQLRETURN result = SQLGetData(statement, index, SQL_C_BIT, &value, sizeof(value), &indicator);
      if (indicator == SQL_NULL_DATA) {
        return nullptr;
      }
      if (!SQL_SUCCEEDED(result)) {
        throw worm::QueryExecutionException(diagnostics(SQL_HANDLE_STMT, statement));
      }
      return value != 0;
    }
    default:
      return textColumn(statement, index);
    }
  }
} // namespace

namespace worm::connection
{
  void MsSqlClient::EnvironmentDeleter::operator()(SQLHENV environment) const noexcept
  {
    if (environment != SQL_NULL_HENV) {
      SQLFreeHandle(SQL_HANDLE_ENV, environment);
    }
  }

  void MsSqlClient::ConnectionDeleter::operator()(SQLHDBC connection) const noexcept
  {
    if (connection != SQL_NULL_HDBC) {
      SQLDisconnect(connection);
      SQLFreeHandle(SQL_HANDLE_DBC, connection);
    }
  }

  MsSqlClient::MsSqlClient(const ConnectionConfig& databaseConfig)
    : Client(databaseConfig.cacheResults),
      environment_(nullptr),
      connection_(nullptr)
  {
    SQLHENV environment = SQL_NULL_HENV;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &environment))) {
      throw DatabaseConnectionException("Failed to allocate ODBC environment.");
    }
    environment_.reset(environment);

    if (!SQL_SUCCEEDED(
          SQLSetEnvAttr(environment_.get(), SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0))) {
      throw DatabaseConnectionException(diagnostics(SQL_HANDLE_ENV, environment_.get()));
    }

    SQLHDBC connection = SQL_NULL_HDBC;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_DBC, environment_.get(), &connection))) {
      throw DatabaseConnectionException(diagnostics(SQL_HANDLE_ENV, environment_.get()));
    }
    connection_.reset(connection);

    std::string connectionString = buildConnectionString(databaseConfig);
    const SQLRETURN result = SQLDriverConnect(connection_.get(),
      nullptr,
      reinterpret_cast<SQLCHAR*>(connectionString.data()),
      SQL_NTS,
      nullptr,
      0,
      nullptr,
      SQL_DRIVER_NOPROMPT);

    if (!SQL_SUCCEEDED(result)) {
      throw DatabaseConnectionException(diagnostics(SQL_HANDLE_DBC, connection_.get()));
    }
  }

  DatabaseType MsSqlClient::type() const noexcept
  {
    return DatabaseType::MSSQL;
  }

  void MsSqlClient::beginTransactionImpl()
  {
    const SQLRETURN result =
      SQLSetConnectAttr(connection_.get(), SQL_ATTR_AUTOCOMMIT, reinterpret_cast<SQLPOINTER>(SQL_AUTOCOMMIT_OFF), 0);

    if (!SQL_SUCCEEDED(result)) {
      throw QueryExecutionException(diagnostics(SQL_HANDLE_DBC, connection_.get()));
    }
  }

  void MsSqlClient::rollbackTransactionImpl()
  {
    if (!SQL_SUCCEEDED(SQLEndTran(SQL_HANDLE_DBC, connection_.get(), SQL_ROLLBACK)) ||
        !SQL_SUCCEEDED(SQLSetConnectAttr(
          connection_.get(), SQL_ATTR_AUTOCOMMIT, reinterpret_cast<SQLPOINTER>(SQL_AUTOCOMMIT_ON), 0))) {
      throw QueryExecutionException(diagnostics(SQL_HANDLE_DBC, connection_.get()));
    }
  }

  void MsSqlClient::commitTransactionImpl()
  {
    if (!SQL_SUCCEEDED(SQLEndTran(SQL_HANDLE_DBC, connection_.get(), SQL_COMMIT)) ||
        !SQL_SUCCEEDED(SQLSetConnectAttr(
          connection_.get(), SQL_ATTR_AUTOCOMMIT, reinterpret_cast<SQLPOINTER>(SQL_AUTOCOMMIT_ON), 0))) {
      throw QueryExecutionException(diagnostics(SQL_HANDLE_DBC, connection_.get()));
    }
  }

  core::ResultSet MsSqlClient::executeImpl(const core::Statement& statement)
  {
    SQLHSTMT rawStatement = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, connection_.get(), &rawStatement))) {
      throw QueryExecutionException(diagnostics(SQL_HANDLE_DBC, connection_.get()));
    }

    std::unique_ptr<void, StatementDeleter> preparedStatement{rawStatement};
    if (!SQL_SUCCEEDED(SQLPrepare(preparedStatement.get(),
          reinterpret_cast<SQLCHAR*>(const_cast<char*>(statement.sql.data())),
          static_cast<SQLINTEGER>(statement.sql.size())))) {
      throw QueryExecutionException(diagnostics(SQL_HANDLE_STMT, preparedStatement.get()));
    }

    std::vector<BoundParameter> parameters;
    parameters.reserve(statement.parameters.size());
    for (const core::Parameter& parameter : statement.parameters) {
      parameters.push_back(makeParameter(parameter));
    }
    refreshParameterPointers(parameters);

    for (std::size_t index = 0; index < parameters.size(); ++index) {
      BoundParameter& parameter = parameters[index];
      const SQLRETURN result = SQLBindParameter(preparedStatement.get(),
        static_cast<SQLUSMALLINT>(index + 1),
        SQL_PARAM_INPUT,
        parameter.valueType,
        parameter.parameterType,
        parameter.columnSize,
        0,
        parameter.value,
        parameter.bufferLength,
        &parameter.indicator);

      if (!SQL_SUCCEEDED(result)) {
        throw QueryExecutionException(diagnostics(SQL_HANDLE_STMT, preparedStatement.get()));
      }
    }

    if (!SQL_SUCCEEDED(SQLExecute(preparedStatement.get()))) {
      throw QueryExecutionException(diagnostics(SQL_HANDLE_STMT, preparedStatement.get()));
    }

    SQLSMALLINT columnCount = 0;
    if (!SQL_SUCCEEDED(SQLNumResultCols(preparedStatement.get(), &columnCount))) {
      throw QueryExecutionException(diagnostics(SQL_HANDLE_STMT, preparedStatement.get()));
    }

    std::vector<core::ResultRow> rows;
    while (columnCount > 0) {
      const SQLRETURN fetchResult = SQLFetch(preparedStatement.get());
      if (fetchResult == SQL_NO_DATA) {
        break;
      }
      if (!SQL_SUCCEEDED(fetchResult)) {
        throw QueryExecutionException(diagnostics(SQL_HANDLE_STMT, preparedStatement.get()));
      }

      std::vector<core::ResultColumn> columns;
      columns.reserve(static_cast<std::size_t>(columnCount));

      for (SQLUSMALLINT index = 1; index <= static_cast<SQLUSMALLINT>(columnCount); ++index) {
        std::array<SQLCHAR, 256> name{};
        SQLSMALLINT nameLength = 0;
        SQLSMALLINT dataType = 0;
        SQLULEN columnSize = 0;
        SQLSMALLINT decimalDigits = 0;
        SQLSMALLINT nullable = 0;

        if (!SQL_SUCCEEDED(SQLDescribeCol(preparedStatement.get(),
              index,
              name.data(),
              static_cast<SQLSMALLINT>(name.size()),
              &nameLength,
              &dataType,
              &columnSize,
              &decimalDigits,
              &nullable))) {
          throw QueryExecutionException(diagnostics(SQL_HANDLE_STMT, preparedStatement.get()));
        }

        const std::size_t safeNameLength =
          (std::min)(static_cast<std::size_t>(nameLength), name.size() - std::size_t{1});
        columns.push_back({std::string{reinterpret_cast<const char*>(name.data()), safeNameLength},
          columnValue(preparedStatement.get(), index, dataType)});
      }

      rows.push_back({std::move(columns)});
    }

    SQLLEN affectedRows = 0;
    if (!SQL_SUCCEEDED(SQLRowCount(preparedStatement.get(), &affectedRows))) {
      throw QueryExecutionException(diagnostics(SQL_HANDLE_STMT, preparedStatement.get()));
    }

    return core::ResultSet{
      std::move(rows), affectedRows > 0 ? static_cast<std::uint64_t>(affectedRows) : std::uint64_t{0}};
  }
} // namespace worm::connection
