#pragma once

#include <connection/client-factory.hpp>
#include <errors/database-exception.hpp>
#include <utils/helpers.hpp>
#include <utils/logger.hpp>

#include <cstddef>
#include <cstdlib>
#include <json/json.h>
#include <string>
#include <typeinfo>

#define UseDependencyInjectionWorm                                                                                     \
  template <typename Type> auto getDependencyInjector() noexcept                                                       \
  {                                                                                                                    \
    return worm::DependencyInjector<Type>();                                                                           \
  }

namespace worm
{
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
      Json::Value config;

      for (const char* key : {"host", "username", "password", "dbname", "port"}) {
        if (const char* value = std::getenv(key)) {
          config[key] = value;
        }
      }

      return connection::getInstance(config, connection::databaseTypes.at(database));
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
        throw DatabaseException("Unsupported database type.");
      }

      return type->second;
    }
  };
} // namespace worm
