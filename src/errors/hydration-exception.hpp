#pragma once

#include <errors/mapping-exception.hpp>

namespace worm
{
  class HydrationException : public MappingException
  {
  public:
    using MappingException::MappingException;
  };
} // namespace worm
