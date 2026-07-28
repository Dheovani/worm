#pragma once

#include <errors/database-exception.hpp>

namespace worm
{
  class QueryExecutionException : public DatabaseException
  {
  public:
    using DatabaseException::DatabaseException;
  };
} // namespace worm
