#include <connection/transaction.hpp>

#include <connection/client.hpp>
#include <errors/transaction-exception.hpp>

#include <utility>

namespace worm::connection
{
  Transaction Client::beginTransaction()
  {
    return Transaction(*this);
  }

  Transaction::Transaction(Client& client)
    : client_(&client),
      active_(true)
  {
    client_->beginTransactionImpl();
  }

  Transaction::~Transaction() noexcept
  {
    if (!active_) {
      return;
    }

    try {
      client_->rollbackTransaction();
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
        client_->rollbackTransaction();
      } catch (...) {}
    }

    client_ = std::exchange(other.client_, nullptr);
    active_ = std::exchange(other.active_, false);

    return *this;
  }

  void Transaction::commit()
  {
    ensureActive();
    client_->commitTransaction();
    active_ = false;
  }

  void Transaction::rollback()
  {
    ensureActive();
    client_->rollbackTransaction();
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
