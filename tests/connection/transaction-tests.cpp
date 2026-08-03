#include <connection/transaction.hpp>

#include <connection/client.hpp>
#include <errors/concurrent-access-exception.hpp>
#include <errors/transaction-exception.hpp>

#include <iostream>
#include <stdexcept>
#include <thread>
#include <utility>

namespace
{
  class RecordingClient final : public worm::connection::Client
  {
  public:
    worm::connection::DatabaseType type() const noexcept override
    {
      return worm::connection::DatabaseType::SQLite;
    }

    int begins{};
    int commits{};
    int rollbacks{};
    bool failNextBegin{};

  private:
    worm::core::ResultSet executeImpl(const worm::core::Statement&) override
    {
      return {};
    }

    void beginTransactionImpl() override
    {
      if (std::exchange(failNextBegin, false)) {
        throw std::runtime_error("begin failed");
      }

      ++begins;
    }

    void rollbackTransactionImpl() override
    {
      ++rollbacks;
    }

    void commitTransactionImpl() override
    {
      ++commits;
    }
  };
} // namespace

int main()
{
  RecordingClient committedClient;
  {
    worm::connection::Transaction transaction = committedClient.beginTransaction();
    transaction.commit();

    if (transaction.active()) {
      std::cerr << "Transaction remained active after commit.\n";
      return 1;
    }
  }

  if (committedClient.begins != 1 || committedClient.commits != 1 || committedClient.rollbacks != 0) {
    std::cerr << "Transaction did not commit explicitly without rollback.\n";
    return 1;
  }

  RecordingClient rolledBackClient;
  {
    worm::connection::Transaction transaction = rolledBackClient.beginTransaction();
    transaction.rollback();
  }

  if (rolledBackClient.begins != 1 || rolledBackClient.commits != 0 || rolledBackClient.rollbacks != 1) {
    std::cerr << "Transaction did not rollback explicitly.\n";
    return 1;
  }

  RecordingClient scopedClient;
  {
    worm::connection::Transaction transaction = scopedClient.beginTransaction();
    static_cast<void>(transaction);
  }

  if (scopedClient.begins != 1 || scopedClient.commits != 0 || scopedClient.rollbacks != 1) {
    std::cerr << "Transaction did not rollback automatically when leaving scope.\n";
    return 1;
  }

  RecordingClient movedClient;
  {
    worm::connection::Transaction transaction = movedClient.beginTransaction();
    worm::connection::Transaction movedTransaction = std::move(transaction);

    if (transaction.active() || !movedTransaction.active()) {
      std::cerr << "Transaction move did not transfer ownership.\n";
      return 1;
    }
  }

  if (movedClient.begins != 1 || movedClient.rollbacks != 1) {
    std::cerr << "Moved transaction rolled back an unexpected number of times.\n";
    return 1;
  }

  bool doubleCommitFailed = false;
  try {
    worm::connection::Transaction transaction = committedClient.beginTransaction();
    transaction.commit();
    transaction.commit();
  } catch (const worm::TransactionException&) {
    doubleCommitFailed = true;
  }

  if (!doubleCommitFailed) {
    std::cerr << "Transaction accepted commit after it had already finished.\n";
    return 1;
  }

  RecordingClient nestedClient;
  {
    worm::connection::Transaction transaction = nestedClient.beginTransaction();
    bool nestedTransactionFailed = false;
    try {
      static_cast<void>(nestedClient.beginTransaction());
    } catch (const worm::TransactionException&) {
      nestedTransactionFailed = true;
    }

    if (!nestedTransactionFailed || nestedClient.begins != 1) {
      std::cerr << "Client accepted simultaneous transactions.\n";
      return 1;
    }
  }

  if (nestedClient.rollbacks != 1) {
    std::cerr << "Rejected nested transaction corrupted the active transaction.\n";
    return 1;
  }

  RecordingClient failedBeginClient;
  failedBeginClient.failNextBegin = true;
  try {
    static_cast<void>(failedBeginClient.beginTransaction());
    std::cerr << "Client accepted a failed transaction start.\n";
    return 1;
  } catch (const std::runtime_error&) {}

  {
    auto transaction = failedBeginClient.beginTransaction();
    transaction.commit();
  }

  if (failedBeginClient.begins != 1 || failedBeginClient.commits != 1) {
    std::cerr << "Failed transaction start left the client in an active state.\n";
    return 1;
  }

  RecordingClient crossThreadClient;
  bool crossThreadRejected = false;
  std::thread foreignBegin([&] {
    try {
      static_cast<void>(crossThreadClient.beginTransaction());
    } catch (const worm::ConcurrentAccessException&) {
      crossThreadRejected = true;
    }
  });
  foreignBegin.join();

  if (!crossThreadRejected || crossThreadClient.begins != 0) {
    std::cerr << "Client allowed a transaction to start from a foreign thread.\n";
    return 1;
  }

  RecordingClient foreignCommitClient;
  auto ownerTransaction = foreignCommitClient.beginTransaction();
  bool foreignCommitRejected = false;
  std::thread foreignCommit([&] {
    try {
      ownerTransaction.commit();
    } catch (const worm::ConcurrentAccessException&) {
      foreignCommitRejected = true;
    }
  });
  foreignCommit.join();

  if (!foreignCommitRejected || !ownerTransaction.active() || foreignCommitClient.commits != 0) {
    std::cerr << "Transaction allowed commit from a foreign thread or lost its active state.\n";
    return 1;
  }

  ownerTransaction.rollback();

  return 0;
}
