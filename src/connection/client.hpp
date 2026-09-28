#pragma once

#include <core/model/entity-metadata.hpp>
#include <core/output/result-set.hpp>
#include <core/persistence/cache.hpp>
#include <core/query/statement.hpp>
#include <core/query/validator.hpp>

#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <thread>

namespace worm::core
{
  template <typename T>
  class Repository;

  class MigrationLock;
} // namespace worm::core

namespace worm::connection
{
  class SchemaInspector;
  class Transaction;

  enum class DatabaseType : std::uint8_t
  {
    PostgreSQL,
    MySQL,
    SQLite,
    MSSQL
  };

  inline const std::map<std::string, DatabaseType> databaseTypes{
    {"postgresql", DatabaseType::PostgreSQL},
    {"mysql", DatabaseType::MySQL},
    {"sqlite", DatabaseType::SQLite},
    {"mssql", DatabaseType::MSSQL},
  };

  class Client
  {
    template <typename T>
    friend class core::Repository;

    friend class Transaction;
    friend class SchemaInspector;
    friend class core::MigrationLock;

  public:
    virtual ~Client() = default;

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    [[nodiscard]]
    virtual DatabaseType type() const noexcept = 0;

    [[nodiscard]]
    Transaction beginTransaction();

  protected:
    explicit Client(bool cacheResults = false) noexcept
      : cacheResults_(cacheResults)
    {}

    virtual void beginTransactionImpl() = 0;
    virtual void rollbackTransactionImpl() = 0;
    virtual void commitTransactionImpl() = 0;
    virtual void acquireMigrationLockImpl(std::string_view name, std::chrono::milliseconds timeout);
    virtual void releaseMigrationLockImpl(std::string_view name, bool completed);

    [[nodiscard]]
    virtual bool migrationLockOwnsTransactionImpl() const noexcept;

  private:
    [[nodiscard]]
    core::ResultSet execute(const core::Statement& statement)
    {
      ensureThreadAffinity();

      const bool cacheable =
        cacheResults_ && !transactionActive_ && migrationLockName_.empty() && core::isSelect(statement.sql);
      if (cacheable) {
        if (const auto cachedResult = cachedResults_.get(statement)) {
          return cachedResult->get();
        }
      } else if (!core::isSelect(statement.sql)) {
        cachedResults_.clear();
      }

      core::ResultSet result = executeImpl(statement);
      if (cacheable) {
        cachedResults_.add(statement, result);
      }

      return result;
    }

    void startTransaction();
    void commitActiveTransaction();
    void rollbackActiveTransaction();
    void acquireMigrationLock(std::string_view name, std::chrono::milliseconds timeout);
    void releaseMigrationLock(bool completed);
    void ensureThreadAffinity() const;

    [[nodiscard]]
    virtual core::ResultSet executeImpl(const core::Statement& statement) = 0;

    core::Cache<core::Statement, core::ResultSet, core::StatementHash> cachedResults_;
    const std::thread::id ownerThread_ = std::this_thread::get_id();
    const bool cacheResults_ = false;
    bool transactionActive_ = false;
    std::string migrationLockName_;
  };

} // namespace worm::connection
