#include <core/persistence/migration-lock.hpp>

#include <errors/migration-lock-exception.hpp>

#include <utility>

namespace worm::core
{
  MigrationLock::MigrationLock(
    std::shared_ptr<connection::Client> client,
    std::string name,
    std::chrono::milliseconds timeout)
    : client_(std::move(client))
  {
    if (!client_) {
      throw MigrationLockException("Migration lock requires a valid client.");
    }

    client_->acquireMigrationLock(name, timeout);
    active_ = true;
  }

  MigrationLock::~MigrationLock() noexcept
  {
    releaseNoThrow();
  }

  MigrationLock::MigrationLock(MigrationLock&& other) noexcept
    : client_(std::move(other.client_)),
      active_(std::exchange(other.active_, false))
  {}

  MigrationLock& MigrationLock::operator=(MigrationLock&& other) noexcept
  {
    if (this == &other) {
      return *this;
    }

    releaseNoThrow();
    client_ = std::move(other.client_);
    active_ = std::exchange(other.active_, false);
    return *this;
  }

  void MigrationLock::release()
  {
    if (!active_ || !client_) {
      throw MigrationLockException("Migration lock has already been released.");
    }

    client_->releaseMigrationLock(true);
    active_ = false;
  }

  bool MigrationLock::active() const noexcept
  {
    return active_;
  }

  void MigrationLock::releaseNoThrow() noexcept
  {
    if (!active_ || !client_) {
      return;
    }

    try {
      client_->releaseMigrationLock(false);
      active_ = false;
    } catch (...) {}
  }
} // namespace worm::core
