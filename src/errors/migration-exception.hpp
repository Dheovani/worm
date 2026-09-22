#pragma once

#include <errors/worm-exception.hpp>

namespace worm
{
  class MigrationException : public WormException
  {
  public:
    using WormException::WormException;
  };
} // namespace worm
