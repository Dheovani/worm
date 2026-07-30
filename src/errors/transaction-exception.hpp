#pragma once

#include <errors/database-exception.hpp>

namespace worm
{
  class TransactionException : public DatabaseException
  {
  public:
    using DatabaseException::DatabaseException;
  };
} // namespace worm
