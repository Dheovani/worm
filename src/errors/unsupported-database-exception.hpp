#pragma once

#include <errors/configuration-exception.hpp>

namespace worm
{
  class UnsupportedDatabaseException : public ConfigurationException
  {
  public:
    using ConfigurationException::ConfigurationException;
  };
} // namespace worm
