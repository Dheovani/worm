#pragma once

#include <errors/invalid-arg-exception.hpp>

namespace worm
{
  class SqlBuildException : public InvalidArgException
  {
  public:
    using InvalidArgException::InvalidArgException;
  };
} // namespace worm
