#pragma once

#include <core/output/result-set.hpp>
#include <core/query/statement.hpp>

#include <cstdint>
#include <map>
#include <string>

namespace worm::connection
{
  enum class DatabaseType : std::uint8_t
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

  class Client
  {
  public:
    virtual ~Client() = default;

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    [[nodiscard]]
    virtual core::ResultSet executeQuery(const core::Statement& statement) const = 0;

  protected:
    Client() = default;

    [[nodiscard]]
    bool isSelect(const std::string& query) const;
  };
} // namespace worm::connection
