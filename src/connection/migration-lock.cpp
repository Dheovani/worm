#include <connection/client.hpp>

#include <errors/invalid-arg-exception.hpp>
#include <errors/migration-lock-exception.hpp>
#include <errors/worm-exception.hpp>

#include <chrono>
#include <string>
#include <string_view>

namespace worm::connection
{
  void Client::acquireMigrationLockImpl(std::string_view name, std::chrono::milliseconds timeout)
  {
    static_cast<void>(timeout);
    throw MigrationLockException("Database driver does not support migration lock '{}'.", name);
  }

  void Client::releaseMigrationLockImpl(std::string_view name, bool completed)
  {
    static_cast<void>(completed);
    throw MigrationLockException("Database driver does not support releasing migration lock '{}'.", name);
  }

  bool Client::migrationLockOwnsTransactionImpl() const noexcept
  {
    return false;
  }

  void Client::acquireMigrationLock(std::string_view name, std::chrono::milliseconds timeout)
  {
    ensureThreadAffinity();
    if (name.empty()) {
      throw InvalidArgException("Migration lock name cannot be empty.");
    }

    if (timeout.count() < 0) {
      throw InvalidArgException("Migration lock timeout cannot be negative.");
    }

    if (!migrationLockName_.empty()) {
      throw MigrationLockException("Client already owns migration lock '{}'.", migrationLockName_);
    }

    if (transactionActive_) {
      throw MigrationLockException("Cannot acquire a migration lock while a transaction is active.");
    }

    try {
      acquireMigrationLockImpl(name, timeout);
      migrationLockName_ = name;
      cachedResults_.clear();
    } catch (const MigrationLockException&) {
      throw;
    } catch (const WormException& error) {
      throw MigrationLockException("Unable to acquire migration lock '{}': {}", name, error.what());
    }
  }

  void Client::releaseMigrationLock(bool completed)
  {
    ensureThreadAffinity();
    if (migrationLockName_.empty()) {
      throw MigrationLockException("Client does not own a migration lock.");
    }

    try {
      releaseMigrationLockImpl(migrationLockName_, completed);
      migrationLockName_.clear();
      cachedResults_.clear();
    } catch (const MigrationLockException&) {
      throw;
    } catch (const WormException& error) {
      throw MigrationLockException("Unable to release migration lock '{}': {}", migrationLockName_, error.what());
    }
  }
} // namespace worm::connection
