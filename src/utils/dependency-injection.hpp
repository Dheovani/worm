#pragma once

#include <connection/configuration.hpp>
#include <errors/unsupported-database-exception.hpp>
#include <utils/helpers.hpp>
#include <utils/logger.hpp>

#include <cstddef>
#include <cstdlib>
#include <string>
#include <typeinfo>

#define UseDependencyInjectionWorm                                                                                     \
  template <typename Type> auto getDependencyInjector() noexcept                                                       \
  {                                                                                                                    \
    return worm::DependencyInjector<Type>();                                                                           \
  }

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

  template <typename Type> class DependencyInjector
  {
  public:
    [[nodiscard]] Type get() const
    {
      return Type{};
    }
  };

  template <> class DependencyInjector<Logger>
  {
  public:
    template <typename Class, std::size_t Index>
    [[nodiscard]]
    Logger get() const
    {
      return {typeid(Class).name(), static_cast<int>(Index)};
    }
  };

  template <> class DependencyInjector<connection::Client>
  {
  public:
    [[nodiscard]]
    connection::Client& get() const
    {
      const std::string database = utils::env::getDatabaseType();
      const connection::ConnectionConfig config = {
        .host = detail::envValue("host"),
        .username = detail::envValue("username"),
        .password = detail::envValue("password"),
        .dbname = detail::envValue("dbname"),
        .port = detail::envValue("port")};

      const auto type = connection::databaseTypes.find(database);
      if (type == connection::databaseTypes.end()) {
        throw UnsupportedDatabaseException("Unsupported database type: " + database);
      }

      return connection::getInstance(config, type->second);
    }
  };

  template <> class DependencyInjector<connection::DatabaseType>
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
} // namespace worm
