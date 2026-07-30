#pragma once

#include <errors/worm-exception.hpp>

namespace worm
{
  class TransactionException : public WormException
  {
  public:
    using WormException::WormException;
  };
} // namespace worm
