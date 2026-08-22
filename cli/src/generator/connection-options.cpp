#include "connection-options.hpp"

#include <utils/dependency-injection.hpp>

#include "../errors/invalid-cli-argument-exception.hpp"

namespace worm::cli::generator
{
  namespace
  {
    [[nodiscard]]
    std::string defaultPort(connection::DatabaseType type)
    {
      switch (type) {
      case connection::DatabaseType::PostgreSQL:
        return "5432";
      case connection::DatabaseType::MySQL:
        return "3306";
      case connection::DatabaseType::MSSQL:
        return "1433";
      case connection::DatabaseType::SQLite:
        return {};
      }
      return {};
    }
  } // namespace

  connection::DatabaseType databaseType(const Invocation& invocation)
  {
    if (!invocation.global.driver.has_value()) {
      throw InvalidCliArgumentException("The command requires a database driver.");
    }

    const auto type = connection::databaseTypes.find(*invocation.global.driver);
    if (type == connection::databaseTypes.end()) {
      throw InvalidCliArgumentException("Unsupported database driver '{}'.", *invocation.global.driver);
    }
    return type->second;
  }

  std::string defaultSchema(connection::DatabaseType type)
  {
    switch (type) {
    case connection::DatabaseType::PostgreSQL:
      return "public";
    case connection::DatabaseType::MySQL:
      return {};
    case connection::DatabaseType::SQLite:
      return "main";
    case connection::DatabaseType::MSSQL:
      return "dbo";
    }
    return {};
  }

  connection::ConnectionConfig connectionConfig(const Invocation& invocation, connection::DatabaseType type)
  {
    if (!invocation.global.database.has_value()) {
      throw InvalidCliArgumentException("The command requires a database name or SQLite path.");
    }

    return {
      .host = invocation.global.host.value_or("localhost"),
      .username = invocation.global.username.value_or(""),
      .password = invocation.global.password.value_or(""),
      .dbname = *invocation.global.database,
      .port = invocation.global.port.value_or(defaultPort(type)),
    };
  }

  connection::SchemaInspector schemaInspector(const Invocation& invocation)
  {
    const auto type = databaseType(invocation);
    return DependencyInjector<connection::SchemaInspector>::get(connectionConfig(invocation, type), type);
  }
} // namespace worm::cli::generator
