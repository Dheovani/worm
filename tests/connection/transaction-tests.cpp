#include <connection/transaction.hpp>

#include <connection/client.hpp>
#include <errors/transaction-exception.hpp>

#include <iostream>
#include <utility>

namespace
{
  class RecordingClient final : public worm::connection::Client
  {
  public:
    worm::core::ResultSet execute(const worm::core::Statement&) override
    {
      return {};
    }

    worm::connection::DatabaseType type() const noexcept override
    {
      return worm::connection::DatabaseType::SQLite;
    }

    int begins{};
    int commits{};
    int rollbacks{};

  private:
    void beginTransactionImpl() override
    {
      ++begins;
    }

    void rollbackTransaction() override
    {
      ++rollbacks;
    }

    void commitTransaction() override
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

  return 0;
}
