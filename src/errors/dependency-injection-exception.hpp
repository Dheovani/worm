#pragma once

#include <errors/worm-exception.hpp>

namespace worm
{
  class DependencyInjectionException : public WormException
  {
  public:
    using WormException::WormException;
  };
} // namespace worm
