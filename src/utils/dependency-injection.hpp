#pragma once

#include <connection/client.hpp>
#include <connection/configuration.hpp>
#include <connection/schema-inspector.hpp>
#include <core/query/dialect.hpp>
#include <core/query/query-builder.hpp>
#include <core/query/sql-builder.hpp>
#include <errors/invalid-arg-exception.hpp>
#include <errors/missing-configuration-exception.hpp>
#include <errors/unregistered-dependency-exception.hpp>
#include <errors/unsupported-database-exception.hpp>
#include <utils/helpers.hpp>
#include <utils/logger.hpp>

#include <chrono>
#include <concepts>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
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
        throw UnregisteredDependencyException("Dependency is not registered: {}", typeid(Type).name());
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
  struct DependencyInjector<connection::TimeoutConfig>
  {
    [[nodiscard]]
    static connection::TimeoutConfig get()
    {
      std::optional<std::chrono::milliseconds> cTimeout = std::nullopt;
      std::optional<std::chrono::milliseconds> qTimeout = std::nullopt;

      if (utils::env::hasValue("CONNECTION_TIMEOUT_MS")) {
        cTimeout = parseTimeout("CONNECTION_TIMEOUT_MS");
      }

      if (utils::env::hasValue("QUERY_TIMEOUT_MS")) {
        qTimeout = parseTimeout("QUERY_TIMEOUT_MS");
      }

      return {
        .connectionTimeout = cTimeout,
        .queryTimeout = qTimeout,
        .cancelOnTimeout = cTimeout.has_value() || qTimeout.has_value(),
      };
    }

  private:
    [[nodiscard]]
    static std::chrono::milliseconds parseTimeout(const char* key)
    {
      try {
        const long long value = std::stoll(utils::env::envValue(key));
        if (value < 0) {
          throw InvalidArgException("{} cannot be negative.", key);
        }

        return std::chrono::milliseconds{value};
      } catch (const InvalidArgException&) {
        throw;
      } catch (const std::exception&) {
        throw InvalidArgException("{} must be a timeout in milliseconds.", key);
      }
    }
  };

  template <>
  struct DependencyInjector<connection::ConnectionConfig>
  {
    [[nodiscard]]
    static connection::ConnectionConfig get()
    {
      return {
        .host = utils::env::envValue("HOST"),
        .username = utils::env::envValue("USERNAME"),
        .password = utils::env::envValue("PASSWORD"),
        .dbname = utils::env::envValue("DBNAME"),
        .port = utils::env::envValue("PORT"),
        .cacheResults = utils::env::envValue("QUERY_CACHE_ENABLED") == "true",
        .timeoutConfig = DependencyInjector<connection::TimeoutConfig>::get(),
      };
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

      return get(database);
    }

    [[nodiscard]]
    static connection::DatabaseType get(std::string_view database)
    {
      const auto type = connection::databaseTypes.find(std::string{database});
      if (type == connection::databaseTypes.end()) {
        throw UnsupportedDatabaseException("Unsupported database type: {}", database);
      }

      return type->second;
    }
  };

  template <>
  struct DependencyInjector<connection::Client>
  {
    [[nodiscard]]
    static std::unique_ptr<connection::Client>
    get(const connection::ConnectionConfig& config, connection::DatabaseType dbType)
    {
      return connection::makeClient(config, dbType);
    }

    [[nodiscard]]
    static std::unique_ptr<connection::Client> get()
    {
      return get(
        DependencyInjector<connection::ConnectionConfig>::get(),
        DependencyInjector<connection::DatabaseType>::get());
    }
  };

  template <>
  struct DependencyInjector<core::Dialect>
  {
    [[nodiscard]]
    static const core::Dialect& get()
    {
      return get(DependencyInjector<connection::DatabaseType>::get());
    }

    [[nodiscard]]
    static const core::Dialect& get(connection::DatabaseType dbType)
    {
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

      if (dbType == connection::DatabaseType::MSSQL) {
        static const core::SqlServerDialect dialect{};
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
      return get(DependencyInjector<connection::DatabaseType>::get());
    }

    [[nodiscard]]
    static const core::SqlBuilder& get(connection::DatabaseType dbType)
    {
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

      if (dbType == connection::DatabaseType::MSSQL) {
        static const core::SqlServerBuilder builder{};
        return builder;
      }

      throw UnsupportedDatabaseException("Unsupported database type.");
    }
  };

  template <>
  struct DependencyInjector<core::QueryBuilder>
  {
    [[nodiscard]]
    static core::QueryBuilder get()
    {
      return core::QueryBuilder{DependencyInjector<core::SqlBuilder>::get()};
    }

    [[nodiscard]]
    static core::QueryBuilder get(connection::DatabaseType dbType)
    {
      return core::QueryBuilder{DependencyInjector<core::SqlBuilder>::get(dbType)};
    }
  };

  template <>
  struct DependencyInjector<connection::SchemaInspector>
  {
    [[nodiscard]]
    static connection::SchemaInspector get(const connection::ConnectionConfig& config, connection::DatabaseType type)
    {
      return connection::SchemaInspector{DependencyInjector<connection::Client>::get(config, type)};
    }

    [[nodiscard]]
    static connection::SchemaInspector get()
    {
      return get(
        DependencyInjector<connection::ConnectionConfig>::get(),
        DependencyInjector<connection::DatabaseType>::get());
    }
  };

} // namespace worm
