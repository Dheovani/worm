#include <core/persistence/migration-history-repository.hpp>
#include <core/persistence/migration-lock.hpp>

#include <connection/client.hpp>
#include <connection/transaction.hpp>
#include <core/query/query-builder.hpp>
#include <core/query/sql-builder.hpp>
#include <errors/invalid-arg-exception.hpp>
#include <errors/migration-lock-exception.hpp>
#include <errors/transaction-exception.hpp>

#include <chrono>
#include <cstddef>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

namespace
{
  class LockingClient final : public worm::connection::Client
  {
  public:
    std::string acquiredName;
    std::chrono::milliseconds acquiredTimeout{};
    std::size_t acquisitions{};
    std::size_t releases{};
    bool rejectAcquisition{false};
    bool lastReleaseCompleted{false};
    bool lockOwnsTransaction{true};

    [[nodiscard]]
    worm::connection::DatabaseType type() const noexcept override
    {
      return worm::connection::DatabaseType::SQLite;
    }

  private:
    worm::core::ResultSet executeImpl(const worm::core::Statement&) override
    {
      return {};
    }

    void beginTransactionImpl() override {}
    void rollbackTransactionImpl() override {}
    void commitTransactionImpl() override {}

    void acquireMigrationLockImpl(std::string_view name, std::chrono::milliseconds timeout) override
    {
      if (rejectAcquisition) {
        throw worm::MigrationLockException("Lock is held by another client.");
      }
      acquiredName = name;
      acquiredTimeout = timeout;
      ++acquisitions;
    }

    void releaseMigrationLockImpl(std::string_view name, bool completed) override
    {
      if (name != acquiredName) {
        throw worm::MigrationLockException("Unexpected lock name.");
      }
      lastReleaseCompleted = completed;
      ++releases;
    }

    [[nodiscard]]
    bool migrationLockOwnsTransactionImpl() const noexcept override
    {
      return lockOwnsTransaction;
    }
  };
} // namespace

int main()
{
  const auto client = std::make_shared<LockingClient>();
  const worm::core::SqliteBuilder sqlBuilder;
  const worm::core::QueryBuilder queryBuilder{sqlBuilder};
  const worm::core::Repository<worm::core::MigrationHistory> repository{client, queryBuilder, "main"};

  {
    worm::core::MigrationLock lock = repository.acquireLock(std::chrono::milliseconds{250});
    if (!lock.active() || client->acquisitions != 1 || client->acquiredName != "worm:migrations:main" ||
        client->acquiredTimeout != std::chrono::milliseconds{250}) {
      std::cerr << "Migration lock did not preserve its name, timeout, or active state.\n";
      return 1;
    }

    bool duplicateRejected = false;
    try {
      static_cast<void>(repository.acquireLock(std::chrono::milliseconds{0}));
    } catch (const worm::MigrationLockException&) {
      duplicateRejected = true;
    }
    if (!duplicateRejected) {
      std::cerr << "Migration lock allowed the same client to acquire two locks.\n";
      return 1;
    }
  }

  if (client->releases != 1 || client->lastReleaseCompleted) {
    std::cerr << "Migration lock did not release on scope exit.\n";
    return 1;
  }

  worm::core::MigrationLock explicitLock = repository.acquireLock();
  bool transactionDuringLockRejected = false;
  try {
    static_cast<void>(client->beginTransaction());
  } catch (const worm::TransactionException&) {
    transactionDuringLockRejected = true;
  }
  explicitLock.release();
  if (!transactionDuringLockRejected || explicitLock.active() || client->releases != 2 ||
      !client->lastReleaseCompleted) {
    std::cerr << "Migration lock did not release explicitly.\n";
    return 1;
  }

  bool repeatedReleaseRejected = false;
  try {
    explicitLock.release();
  } catch (const worm::MigrationLockException&) {
    repeatedReleaseRejected = true;
  }

  bool negativeTimeoutRejected = false;
  try {
    static_cast<void>(repository.acquireLock(std::chrono::milliseconds{-1}));
  } catch (const worm::InvalidArgException&) {
    negativeTimeoutRejected = true;
  }

  const worm::core::Repository<worm::core::MigrationHistory> longSchemaRepository{
    client,
    queryBuilder,
    std::string(80, 'a'),
  };
  worm::core::MigrationLock boundedNameLock = longSchemaRepository.acquireLock();
  const bool boundedName = client->acquiredName.starts_with("worm:migrations:") && client->acquiredName.size() <= 64;
  boundedNameLock.release();

  bool lockDuringTransactionRejected = false;
  {
    auto transaction = client->beginTransaction();
    try {
      static_cast<void>(repository.acquireLock(std::chrono::milliseconds{0}));
    } catch (const worm::MigrationLockException&) {
      lockDuringTransactionRejected = true;
    }
    transaction.rollback();
  }

  client->rejectAcquisition = true;
  bool contentionRejected = false;
  try {
    static_cast<void>(repository.acquireLock(std::chrono::milliseconds{0}));
  } catch (const worm::MigrationLockException&) {
    contentionRejected = true;
  }

  client->rejectAcquisition = false;
  client->lockOwnsTransaction = false;
  worm::core::MigrationLock advisoryLock = repository.acquireLock();
  auto transaction = client->beginTransaction();
  transaction.rollback();
  advisoryLock.release();

  if (!repeatedReleaseRejected || !negativeTimeoutRejected || !boundedName || !lockDuringTransactionRejected ||
      !contentionRejected) {
    std::cerr << "Migration lock accepted an invalid lifecycle or failed acquisition.\n";
    return 1;
  }

  return 0;
}
