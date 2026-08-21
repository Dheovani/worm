#pragma once

#include "worm-cli-exception.hpp"

namespace worm::cli
{
  class UnknownArgumentException : public WormCliException
  {
  public:
    using WormCliException::WormCliException;
  };
} // namespace worm::cli
