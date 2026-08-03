#pragma once

#include <connection/client.hpp>
#include <connection/configuration.hpp>
#include <core/query/dialect.hpp>
#include <core/query/sql-builder.hpp>
#include <errors/unregistered-dependency-exception.hpp>
#include <errors/unsupported-database-exception.hpp>
#include <utils/helpers.hpp>
#include <utils/logger.hpp>

#include <concepts>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <string>
#include <typeinfo>

namespace worm
{

  namespace detail
  {
    [[nodiscard]]
    inline std::string envValue(const char* key)
    {
      if (const char* value = std::getenv(key)) {
        return value;
      }

      return {};
    }
  } // namespace detail

  template <typename Type>
  class DependencyInjector
  {
  public:
    [[nodiscard]] Type get() const
    {
      if constexpr (std::default_initializable<Type>) {
        return Type{};
      } else {
        throw UnregisteredDependencyException("Dependency is not registered: " + std::string{typeid(Type).name()});
      }
    }
  };

  template <>
  class DependencyInjector<Logger>
  {
  public:
    template <typename Class, std::size_t Index>
    [[nodiscard]]
    Logger get() const
    {
      return {typeid(Class).name(), static_cast<int>(Index)};
    }
  };

  template <>
  class DependencyInjector<connection::ConnectionConfig>
  {
  public:
    [[nodiscard]]
    connection::ConnectionConfig get() const
    {
      return {
        .host = detail::envValue("HOST"),
        .username = detail::envValue("USERNAME"),
        .password = detail::envValue("PASSWORD"),
        .dbname = detail::envValue("DBNAME"),
        .port = detail::envValue("PORT")};
    }
  };

  template <>
  class DependencyInjector<connection::DatabaseType>
  {
  public:
    [[nodiscard]]
    connection::DatabaseType get() const
    {
      const std::string database = utils::env::getDatabaseType();
      const auto type = connection::databaseTypes.find(database);
      if (type == connection::databaseTypes.end()) {
        throw UnsupportedDatabaseException("Unsupported database type: " + database);
      }

      return type->second;
    }
  };

  template <>
  class DependencyInjector<connection::Client>
  {
  public:
    [[nodiscard]]
    connection::Client& get() const
    {
      static std::unique_ptr<connection::Client> client = connection::makeClient(
        DependencyInjector<connection::ConnectionConfig>().get(),
        DependencyInjector<connection::DatabaseType>().get());

      return *client;
    }
  };

  template <>
  class DependencyInjector<core::Dialect>
  {
  public:
    [[nodiscard]]
    const core::Dialect& get() const
    {
      const auto dbType = DependencyInjector<connection::DatabaseType>().get();

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
  class DependencyInjector<core::SqlBuilder>
  {
  public:
    [[nodiscard]]
    const core::SqlBuilder& get() const
    {
      const auto dbType = DependencyInjector<connection::DatabaseType>().get();

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
