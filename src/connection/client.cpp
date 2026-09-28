#include <connection/client.hpp>

#include <errors/query-execution-exception.hpp>

namespace worm::connection
{
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
