#include <core/query/statement.hpp>

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

  const worm::core::Statement statement{"select * from users where id = $1", {std::int64_t{7}}};

  if (statement.sql != "select * from users where id = $1" ||
      statement.parameters != std::vector<worm::core::Parameter>{std::int64_t{7}}) {
    std::cerr << "Statement did not preserve SQL and parameters.\n";
    return 1;
  }

  const worm::core::Statement fromStatement = worm::core::Statement::prepare(
    "select * from users where email = ? and active = ?", {std::string{"ada@example.com"}, true});

  if (fromStatement.sql != "select * from users where email = ? and active = ?" ||
      fromStatement.parameters != std::vector<worm::core::Parameter>{std::string{"ada@example.com"}, true}) {
    std::cerr << "Statement factory did not preserve manual SQL and bound parameters.\n";
    return 1;
  }

  if (!worm::core::hasFilterWhere("delete from users where users.id = ?", "users") ||
      !worm::core::hasFilterWhere("delete from users u where u.id = ?", "u") ||
      !worm::core::hasFilterWhere("delete from `users` `u` where `u`.`id` = ?", "u")) {
    std::cerr << "Statement filter detection did not accept qualified DELETE filters.\n";
    return 1;
  }

  if (worm::core::hasFilterWhere("delete from users", "users") ||
      worm::core::hasFilterWhere("delete from users where true", "users") ||
      worm::core::hasFilterWhere("delete from users u where users.id = ?", "u")) {
    std::cerr << "Statement filter detection accepted unsafe DELETE filters.\n";
    return 1;
  }

  return 0;
}
