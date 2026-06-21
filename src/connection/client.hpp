#pragma once

#include <json/json.h>

#include <map>
#include <string>

namespace worm
{
  enum class DatabaseType
  {
    PostgreSQL,
    MySQL,
    SQLite
  };

  inline const std::map<std::string, DatabaseType> databaseTypes{
    {"postgresql", DatabaseType::PostgreSQL},
    {"mysql", DatabaseType::MySQL},
    {"sqlite", DatabaseType::SQLite},
  };
  
  namespace connection
  {
    class Client
    {
    public:
      virtual ~Client() = default;

      Client(const Client&) = delete;
      Client& operator=(const Client&) = delete;

      virtual Json::Value executeQuery(const std::string& query) const = 0;

    protected:
      Client() = default;

      [[nodiscard]] bool isSelect(const std::string& query) const;
    };
  } // namespace connection
} // namespace worm
