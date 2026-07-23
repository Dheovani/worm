#include <core/statement.hpp>

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

int main()
{
  const worm::core::Statement emptyStatement;
  if (!emptyStatement.sql.empty() || !emptyStatement.parameters.empty()) {
    std::cerr << "Default statement should be empty.\n";
    return 1;
  }

  const worm::core::Statement statement{
    "select * from users where id = $1",
    {std::int64_t{7}}};

  if (statement.sql != "select * from users where id = $1" ||
      statement.parameters != std::vector<worm::core::Parameter>{std::int64_t{7}}) {
    std::cerr << "Statement did not preserve SQL and parameters.\n";
    return 1;
  }

  return 0;
}
