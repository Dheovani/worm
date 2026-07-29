#include <core/query/validator.hpp>

#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace
{
  struct ValidationCase
  {
    std::string query;
    bool insert;
    bool update;
    bool delete_;
    bool select;
  };
} // namespace

int main()
{
  const std::vector<ValidationCase> cases{
    {"INSERT INTO users VALUES (1)", true, false, false, false},
    {"  insert into users values (1)", true, false, false, false},
    {"UPDATE users SET name = ?", false, true, false, false},
    {"  update users set name = ?", false, true, false, false},
    {"DELETE FROM users WHERE id = ?", false, false, true, false},
    {"  delete from users where id = ?", false, false, true, false},
    {"SELECT * FROM users", false, false, false, true},
    {"  SeLeCt 1", false, false, false, true},
    {"WITH users AS (SELECT 1) SELECT * FROM users", false, false, false, false},
    {"", false, false, false, false},
  };

  for (const ValidationCase& current : cases) {
    if (worm::core::isInsert(current.query) != current.insert ||
        worm::core::isUpdate(current.query) != current.update ||
        worm::core::isDelete(current.query) != current.delete_ ||
        worm::core::isSelect(current.query) != current.select) {
      std::cerr << "Unexpected operation validation for query: " << current.query << '\n';
      return 1;
    }
  }

  return 0;
}
