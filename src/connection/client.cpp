#include <connection/client.hpp>

#include <connection/configuration.hpp>
#include <errors/query-execution-exception.hpp>
#include <utils/logger.hpp>

#include <chrono>
#include <string>

namespace worm::connection
{
  namespace
  {
    constexpr auto slowQueryThreshold = std::chrono::seconds{1};
  }

  Client::~Client()
  {
    logger.info("Database connection closed.");
  }

  core::ResultSet Client::execute(const core::Statement& statement)
  {
    ensureThreadAffinity();
    logger.log(
      LogLevel::Trace,
      "SQL execution started.",
      {
        {"driver", std::string{databaseTypeName(type())}},
        {"sql", statement.sql},
        {"binding_count", std::to_string(statement.parameters.size())},
      });

    const bool select = core::isSelect(statement.sql);
    const bool cacheable = cacheResults_ && !transactionActive_ && migrationLockName_.empty() && select;
    if (cacheable) {
      if (const auto cachedResult = cachedResults_.get(statement)) {
        logger.log(
          LogLevel::Debug,
          "SQL result returned from cache.",
          {
            {"driver", std::string{databaseTypeName(type())}},
            {"returned_rows", std::to_string(cachedResult->get().rowCount())},
          });
        return cachedResult->get();
      }
    } else if (!select) {
      cachedResults_.clear();
    }

    const auto started = std::chrono::steady_clock::now();
    core::ResultSet result = executeImpl(statement);
    const auto duration = std::chrono::steady_clock::now() - started;
    if (cacheable) {
      cachedResults_.add(statement, result);
    }

    logger.log(
      LogLevel::Debug,
      "SQL execution finished.",
      {
        {"driver", std::string{databaseTypeName(type())}},
        {"duration_ms", std::to_string(std::chrono::duration<double, std::milli>{duration}.count())},
        {"affected_rows", std::to_string(result.affectedRows())},
        {"returned_rows", std::to_string(result.rowCount())},
      });

    if (duration >= slowQueryThreshold) {
      logger.log(
        LogLevel::Warning,
        "Slow SQL execution detected.",
        {
          {"driver", std::string{databaseTypeName(type())}},
          {"duration_ms", std::to_string(std::chrono::duration<double, std::milli>{duration}.count())},
        });
    }

    return result;
  }

  void Client::ping()
  {
    ensureThreadAffinity();
    const core::ResultSet result = executeImpl(core::Statement{"SELECT 1"});
    if (result.rowCount() != 1) {
      throw QueryExecutionException("Database connectivity check returned an unexpected result.");
    }
  }

  bool Client::isConnected() noexcept
  {
    try {
      ping();
      return true;
    } catch (...) {
      return false;
    }
  }

  DatabaseVersion Client::databaseVersion()
  {
    ensureThreadAffinity();
    return parseDatabaseVersion(databaseVersionImpl());
  }

  DatabasePermissions Client::databasePermissions()
  {
    ensureThreadAffinity();
    return databasePermissionsImpl();
  }

  std::string Client::databaseVersionImpl()
  {
    throw QueryExecutionException("Database version diagnostics are not implemented by this client.");
  }

  DatabasePermissions Client::databasePermissionsImpl()
  {
    throw QueryExecutionException("Database permission diagnostics are not implemented by this client.");
  }
} // namespace worm::connection
