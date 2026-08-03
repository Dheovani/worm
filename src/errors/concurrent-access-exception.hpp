#pragma once

#include <errors/worm-exception.hpp>

namespace worm
{
  class ConcurrentAccessException : public WormException
  {
  public:
    using WormException::WormException;
  };
}
