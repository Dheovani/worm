#pragma once

#include <connection/configuration.hpp>

#ifdef _WIN32
#include <windows.h>
#endif
#include <sql.h>
#include <sqlext.h>

#include <array>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace worm::tests::mssql
{
  inline std::string environmentValue(const char* name, const char* fallback = nullptr)
  {
    if (const char* value = std::getenv(name)) {
      return value;
    }
    return fallback != nullptr ? fallback : "";
  }

  inline connection::ConnectionConfig connectionConfig()
  {
    return {
      .host = environmentValue("WORM_TEST_MSSQL_HOST", "127.0.0.1"),
      .username = environmentValue("WORM_TEST_MSSQL_USERNAME", "sa"),
      .password = environmentValue("WORM_TEST_MSSQL_PASSWORD"),
      .dbname = environmentValue("WORM_TEST_MSSQL_DBNAME"),
      .port = environmentValue("WORM_TEST_MSSQL_PORT", "1433"),
      .trustServerCertificate = true,
    };
  }

  class OdbcConnection
  {
  public:
    explicit OdbcConnection(const connection::ConnectionConfig& config)
    {
      try {
        if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &environment_)) ||
            !SQL_SUCCEEDED(
              SQLSetEnvAttr(environment_, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0)) ||
            !SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_DBC, environment_, &connection_))) {
          throw std::runtime_error("Could not allocate the MSSQL contract ODBC connection.");
        }

        const std::string driver = environmentValue("MSSQL_ODBC_DRIVER", "ODBC Driver 18 for SQL Server");
        std::string connectionString = "Driver={" + driver + "};Server=tcp:" + config.host + ',' + config.port +
                                       ";Database=" + config.dbname + ";UID=" + config.username +
                                       ";PWD=" + config.password + ";Encrypt=yes;TrustServerCertificate=yes;";
        if (!SQL_SUCCEEDED(SQLDriverConnect(
              connection_,
              nullptr,
              reinterpret_cast<SQLCHAR*>(connectionString.data()),
              SQL_NTS,
              nullptr,
              0,
              nullptr,
              SQL_DRIVER_NOPROMPT))) {
          throw std::runtime_error(diagnostics(SQL_HANDLE_DBC, connection_));
        }
      } catch (...) {
        close();
        throw;
      }
    }

    ~OdbcConnection()
    {
      close();
    }

    OdbcConnection(const OdbcConnection&) = delete;
    OdbcConnection& operator=(const OdbcConnection&) = delete;

    void execute(std::string sql) const
    {
      SQLHSTMT statement = SQL_NULL_HSTMT;
      if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, connection_, &statement))) {
        throw std::runtime_error(diagnostics(SQL_HANDLE_DBC, connection_));
      }

      const SQLRETURN result =
        SQLExecDirect(statement, reinterpret_cast<SQLCHAR*>(sql.data()), static_cast<SQLINTEGER>(sql.size()));
      const std::string error = SQL_SUCCEEDED(result) ? "" : diagnostics(SQL_HANDLE_STMT, statement);
      SQLFreeHandle(SQL_HANDLE_STMT, statement);
      if (!error.empty()) {
        throw std::runtime_error(error);
      }
    }

    [[nodiscard]]
    long long scalar(std::string sql) const
    {
      SQLHSTMT statement = SQL_NULL_HSTMT;
      if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, connection_, &statement))) {
        throw std::runtime_error(diagnostics(SQL_HANDLE_DBC, connection_));
      }

      const SQLRETURN result =
        SQLExecDirect(statement, reinterpret_cast<SQLCHAR*>(sql.data()), static_cast<SQLINTEGER>(sql.size()));
      SQLBIGINT value = 0;
      SQLLEN indicator = 0;
      const bool succeeded = SQL_SUCCEEDED(result) && SQL_SUCCEEDED(SQLFetch(statement)) &&
                             SQL_SUCCEEDED(SQLGetData(statement, 1, SQL_C_SBIGINT, &value, sizeof(value), &indicator));
      const std::string error = succeeded ? "" : diagnostics(SQL_HANDLE_STMT, statement);
      SQLFreeHandle(SQL_HANDLE_STMT, statement);
      if (!error.empty()) {
        throw std::runtime_error(error);
      }
      return value;
    }

  private:
    static std::string diagnostics(SQLSMALLINT handleType, SQLHANDLE handle)
    {
      std::array<SQLCHAR, 1024> message{};
      std::array<SQLCHAR, 6> state{};
      SQLINTEGER nativeError = 0;
      SQLSMALLINT length = 0;
      if (!SQL_SUCCEEDED(SQLGetDiagRec(
            handleType,
            handle,
            1,
            state.data(),
            &nativeError,
            message.data(),
            static_cast<SQLSMALLINT>(message.size()),
            &length))) {
        return "ODBC operation failed without diagnostics.";
      }
      return {reinterpret_cast<const char*>(message.data()), static_cast<std::size_t>(length)};
    }

    void close() noexcept
    {
      if (connection_ != SQL_NULL_HDBC) {
        SQLDisconnect(connection_);
        SQLFreeHandle(SQL_HANDLE_DBC, connection_);
        connection_ = SQL_NULL_HDBC;
      }
      if (environment_ != SQL_NULL_HENV) {
        SQLFreeHandle(SQL_HANDLE_ENV, environment_);
        environment_ = SQL_NULL_HENV;
      }
    }

    SQLHENV environment_{SQL_NULL_HENV};
    SQLHDBC connection_{SQL_NULL_HDBC};
  };
} // namespace worm::tests::mssql
