#pragma once

#include <core/output/result-set.hpp>
#include <core/query/statement.hpp>

#include <cstdint>
#include <map>
#include <string>

namespace worm::connection
{
  class Transaction;

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
    friend class Transaction;

  public:
    virtual ~Client() = default;

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    [[nodiscard]]
    virtual core::ResultSet execute(const core::Statement& statement) = 0;

    [[nodiscard]]
    virtual DatabaseType type() const noexcept = 0;

    [[nodiscard]]
    Transaction beginTransaction();

  protected:
    Client() = default;

    virtual void beginTransactionImpl() = 0;
    virtual void rollbackTransaction() = 0;
    virtual void commitTransaction() = 0;
  };

} // namespace worm::connection
