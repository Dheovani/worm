#pragma once

#include <errors/dependency-injection-exception.hpp>

namespace worm
{
  class DependencyResolutionException : public DependencyInjectionException
  {
  public:
    using DependencyInjectionException::DependencyInjectionException;
  };
} // namespace worm
