#pragma once

#include "worm-cli-exception.hpp"

namespace worm::cli
{
  class DuplicateCommandException : public WormCliException
  {
  public:
    using WormCliException::WormCliException;
  };
} // namespace worm::cli
