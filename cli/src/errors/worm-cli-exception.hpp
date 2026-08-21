#pragma once

#include <errors/worm-exception.hpp>

namespace worm::cli
{
  class WormCliException : public WormException
  {
  public:
    using WormException::WormException;
  };
} // namespace worm::cli
