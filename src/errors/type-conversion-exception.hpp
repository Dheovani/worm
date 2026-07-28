#pragma once

#include <errors/mapping-exception.hpp>

namespace worm
{
  class TypeConversionException : public MappingException
  {
  public:
    using MappingException::MappingException;
  };
} // namespace worm
