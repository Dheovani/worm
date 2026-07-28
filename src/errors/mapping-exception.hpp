#pragma once

#include <errors/worm-exception.hpp>

namespace worm
{
  class MappingException : public WormException
  {
  public:
    using WormException::WormException;
  };
} // namespace worm
