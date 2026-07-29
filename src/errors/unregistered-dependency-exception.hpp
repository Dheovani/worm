#pragma once

#include <errors/dependency-resolution-exception.hpp>

namespace worm
{
  class UnregisteredDependencyException : public DependencyResolutionException
  {
  public:
    using DependencyResolutionException::DependencyResolutionException;
  };
} // namespace worm
