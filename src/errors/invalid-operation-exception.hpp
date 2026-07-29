#pragma once

#include <errors/invalid-arg-exception.hpp>

namespace worm
{
  class InvalidOperationException : public InvalidArgException
  {
  public:
    using InvalidArgException::InvalidArgException;
  };
} // namespace worm
