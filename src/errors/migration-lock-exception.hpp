#pragma once

#include <errors/migration-exception.hpp>

namespace worm
{
  class MigrationLockException : public MigrationException
  {
  public:
    using MigrationException::MigrationException;
  };
} // namespace worm
