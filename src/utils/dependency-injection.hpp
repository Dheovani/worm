#pragma once

#include <connection/client.hpp>
#include <errors/database-exception.hpp>
#include <utils/client-factory.hpp>
#include <utils/helpers.hpp>
#include <utils/logger.hpp>

#include <cstddef>
#include <cstdlib>
#include <json/json.h>
#include <string>
#include <typeinfo>

#define WORM_USE_DEPENDENCY_INJECTION                                                              \
  template <typename Type> auto getDependencyInjector() noexcept                                   \
  {                                                                                                \
    return worm::DependencyInjector<Type>();                                                       \
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
    template <typename Class, std::size_t Index> [[nodiscard]] Logger get() const
    {
      return Logger(typeid(Class).name(), static_cast<int>(Index));
    }
  };

  template <> class DependencyInjector<connection::Client>
  {
  public:
    [[nodiscard]] connection::Client& get() const
    {
      const std::string database = utils::env::getDatabaseType();
      Json::Value config;

      for (const char* key : {"host", "username", "password", "dbname", "port"}) {
        if (const char* value = std::getenv(key))
          config[key] = value;
      }

      return worm::getInstance(config, databaseTypes.at(database));
    }
  };

  template <> class DependencyInjector<DatabaseType>
  {
  public:
    [[nodiscard]] DatabaseType get() const
    {
      const std::string database = utils::env::getDatabaseType();
      const auto type = databaseTypes.find(database);
      if (type == databaseTypes.end())
        throw DatabaseException("Unsupported database type.");

      return type->second;
    }
  };
} // namespace worm
