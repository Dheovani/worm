#pragma once

#include <chrono>
#include <memory>
#include <string>

#include <connection/client.hpp>

namespace worm::core
{
  class MigrationLock final
  {
    template <typename T>
    friend class Repository;

  public:
    ~MigrationLock() noexcept;

    MigrationLock(const MigrationLock&) = delete;
    MigrationLock& operator=(const MigrationLock&) = delete;

    MigrationLock(MigrationLock&& other) noexcept;
    MigrationLock& operator=(MigrationLock&& other) noexcept;

    void release();

    [[nodiscard]]
    bool active() const noexcept;

  private:
    MigrationLock(std::shared_ptr<connection::Client> client, std::string name, std::chrono::milliseconds timeout);

    void releaseNoThrow() noexcept;

    std::shared_ptr<connection::Client> client_;
    bool active_{false};
  };
} // namespace worm::core
