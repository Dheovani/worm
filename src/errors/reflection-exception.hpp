#pragma once

#include <errors/worm-exception.hpp>

namespace worm
{
  class ReflectionException : public WormException
  {
  public:
    using WormException::WormException;
  };
} // namespace worm
