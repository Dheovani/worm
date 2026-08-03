#pragma once

#include <core/model/entity-metadata.hpp>
#include <core/output/result-set.hpp>
#include <core/query/statement.hpp>

#include <cstdint>
#include <map>
#include <string>
#include <thread>

namespace worm::core
{
  template <PersistableEntity T>
  class Repository;
}

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
    template <core::PersistableEntity T>
    friend class core::Repository;

    friend class Transaction;

  public:
    virtual ~Client() = default;

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    [[nodiscard]]
    virtual DatabaseType type() const noexcept = 0;

    [[nodiscard]]
    Transaction beginTransaction();

  protected:
    Client() = default;

    virtual void beginTransactionImpl() = 0;
    virtual void rollbackTransactionImpl() = 0;
    virtual void commitTransactionImpl() = 0;

  private:
    [[nodiscard]]
    core::ResultSet execute(const core::Statement& statement)
    {
      ensureThreadAffinity();
      return executeImpl(statement);
    }

    void startTransaction();
    void commitActiveTransaction();
    void rollbackActiveTransaction();
    void ensureThreadAffinity() const;

    [[nodiscard]]
    virtual core::ResultSet executeImpl(const core::Statement& statement) = 0;

    const std::thread::id ownerThread_ = std::this_thread::get_id();
    bool transactionActive_ = false;
  };

} // namespace worm::connection
