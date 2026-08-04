#pragma once

#include <connection/client.hpp>
#include <connection/configuration.hpp>
#include <core/query/dialect.hpp>
#include <core/query/sql-builder.hpp>
#include <errors/missing-configuration-exception.hpp>
#include <errors/unregistered-dependency-exception.hpp>
#include <errors/unsupported-database-exception.hpp>
#include <utils/helpers.hpp>
#include <utils/logger.hpp>

#include <concepts>
#include <cstddef>
#include <memory>
#include <string>
#include <typeinfo>

namespace worm
{

  template <typename Type>
  struct DependencyInjector
  {
    [[nodiscard]]
    static Type get()
    {
      if constexpr (std::default_initializable<Type>) {
        return Type{};
      } else {
        throw UnregisteredDependencyException("Dependency is not registered: " + std::string{typeid(Type).name()});
      }
    }
  };

  template <>
  struct DependencyInjector<Logger>
  {
    template <typename Class, std::size_t Index>
    [[nodiscard]]
    static Logger get()
    {
      return {typeid(Class).name(), static_cast<int>(Index)};
    }
  };

  template <>
  struct DependencyInjector<connection::ConnectionConfig>
  {
    [[nodiscard]]
    static connection::ConnectionConfig get()
    {
      return {.host = utils::env::envValue("HOST"),
        .username = utils::env::envValue("USERNAME"),
        .password = utils::env::envValue("PASSWORD"),
        .dbname = utils::env::envValue("DBNAME"),
        .port = utils::env::envValue("PORT")};
    }
  };

  template <>
  struct DependencyInjector<connection::DatabaseType>
  {
    [[nodiscard]]
    static connection::DatabaseType get()
    {
      const std::string database = utils::env::envValue("DATABASE_TYPE");
      if (database.empty()) {
        throw MissingConfigurationException("The DATABASE_TYPE environment variable is missing.");
      }

      const auto type = connection::databaseTypes.find(database);
      if (type == connection::databaseTypes.end()) {
        throw UnsupportedDatabaseException("Unsupported database type: " + database);
      }

      return type->second;
    }
  };

  template <>
  struct DependencyInjector<connection::Client>
  {
    [[nodiscard]]
    static connection::Client& get()
    {
      static std::unique_ptr<connection::Client> client = connection::makeClient(
        DependencyInjector<connection::ConnectionConfig>::get(), DependencyInjector<connection::DatabaseType>::get());

      return *client;
    }
  };

  template <>
  struct DependencyInjector<core::Dialect>
  {
    [[nodiscard]]
    static const core::Dialect& get()
    {
      const auto dbType = DependencyInjector<connection::DatabaseType>::get();

      if (dbType == connection::DatabaseType::PostgreSQL) {
        static const core::PostgresDialect dialect{};
        return dialect;
      }

      if (dbType == connection::DatabaseType::MySQL) {
        static const core::MySqlDialect dialect{};
        return dialect;
      }

      if (dbType == connection::DatabaseType::SQLite) {
        static const core::SqliteDialect dialect{};
        return dialect;
      }

      throw UnsupportedDatabaseException("Unsupported database type.");
    }
  };

  template <>
  struct DependencyInjector<core::SqlBuilder>
  {
    [[nodiscard]]
    static const core::SqlBuilder& get()
    {
      const auto dbType = DependencyInjector<connection::DatabaseType>::get();

      if (dbType == connection::DatabaseType::PostgreSQL) {
        static const core::PgBuilder builder{};
        return builder;
      }

      if (dbType == connection::DatabaseType::MySQL) {
        static const core::MySqlBuilder builder{};
        return builder;
      }

      if (dbType == connection::DatabaseType::SQLite) {
        static const core::SqliteBuilder builder{};
        return builder;
      }

      throw UnsupportedDatabaseException("Unsupported database type.");
    }
  };

} // namespace worm
