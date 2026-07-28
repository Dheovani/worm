#pragma once

#include <errors/invalid-arg-exception.hpp>

namespace worm
{
  class InvalidArgTypeException : public InvalidArgException
  {
  public:
    using InvalidArgException::InvalidArgException;
  };
} // namespace worm
