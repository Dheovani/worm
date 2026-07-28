#pragma once

#include <errors/configuration-exception.hpp>

namespace worm
{
  class MissingConfigurationException : public ConfigurationException
  {
  public:
    using ConfigurationException::ConfigurationException;
  };
} // namespace worm
