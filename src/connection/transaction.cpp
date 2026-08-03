#include <connection/transaction.hpp>

#include <connection/client.hpp>
#include <errors/concurrent-access-exception.hpp>
#include <errors/transaction-exception.hpp>

#include <utility>

namespace worm::connection
{
  Transaction Client::beginTransaction()
  {
    return Transaction(*this);
  }

  void Client::startTransaction()
  {
    ensureThreadAffinity();
    if (transactionActive_) {
      throw worm::TransactionException("A transaction is already active for this client.");
    }

    beginTransactionImpl();
    transactionActive_ = true;
  }

  void Client::commitActiveTransaction()
  {
    ensureThreadAffinity();
    if (!transactionActive_) {
      throw worm::TransactionException("There is no active transaction to commit.");
    }

    commitTransactionImpl();
    transactionActive_ = false;
  }

  void Client::rollbackActiveTransaction()
  {
    ensureThreadAffinity();
    if (!transactionActive_) {
      throw worm::TransactionException("There is no active transaction to rollback.");
    }

    rollbackTransactionImpl();
    transactionActive_ = false;
  }

  void Client::ensureThreadAffinity() const
  {
    if (std::this_thread::get_id() != ownerThread_) {
      throw worm::ConcurrentAccessException("Client accessed from a different thread than it was created on.");
    }
  }

  Transaction::Transaction(Client& client)
    : client_(&client),
      active_(true)
  {
    client_->startTransaction();
  }

  Transaction::~Transaction() noexcept
  {
    if (!active_) {
      return;
    }

    try {
      client_->rollbackActiveTransaction();
    } catch (...) {}
  }

  Transaction::Transaction(Transaction&& other) noexcept
    : client_(std::exchange(other.client_, nullptr)),
      active_(std::exchange(other.active_, false))
  {}

  Transaction& Transaction::operator=(Transaction&& other) noexcept
  {
    if (this == &other) {
      return *this;
    }

    if (active_ && client_ != nullptr) {
      try {
        client_->rollbackActiveTransaction();
      } catch (...) {}
    }

    client_ = std::exchange(other.client_, nullptr);
    active_ = std::exchange(other.active_, false);

    return *this;
  }

  void Transaction::commit()
  {
    ensureActive();
    client_->commitActiveTransaction();
    active_ = false;
  }

  void Transaction::rollback()
  {
    ensureActive();
    client_->rollbackActiveTransaction();
    active_ = false;
  }

  bool Transaction::active() const noexcept
  {
    return active_;
  }

  void Transaction::ensureActive() const
  {
    if (!active_ || client_ == nullptr) {
      throw worm::TransactionException("Transaction has already finished.");
    }
  }
} // namespace worm::connection
