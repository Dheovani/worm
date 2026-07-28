#pragma once

#include <errors/worm-exception.hpp>

namespace worm
{
  class InvalidArgException : public WormException
  {
  public:
    using WormException::WormException;
  };
} // namespace worm
