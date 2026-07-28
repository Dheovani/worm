#pragma once

#include <errors/database-exception.hpp>

namespace worm
{
  class DatabaseConnectionException : public DatabaseException
  {
  public:
    using DatabaseException::DatabaseException;
  };
} // namespace worm
