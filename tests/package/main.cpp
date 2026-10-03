#include <core/query/statement.hpp>

#include <string>

int main()
{
  const worm::core::Statement statement{"SELECT 1"};
  return statement.sql == "SELECT 1" ? 0 : 1;
}
