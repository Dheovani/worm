#include <core/query/statement.hpp>

#ifndef WORM_HAS_POSTGRESQL_DRIVER
#error "The PostgreSQL feature was not propagated by the Worm vcpkg port."
#endif
#ifndef WORM_HAS_MYSQL_DRIVER
#error "The MySQL feature was not propagated by the Worm vcpkg port."
#endif
#ifndef WORM_HAS_SQLITE_DRIVER
#error "The SQLite feature was not propagated by the Worm vcpkg port."
#endif
#ifndef WORM_HAS_MSSQL_DRIVER
#error "The MSSQL feature was not propagated by the Worm vcpkg port."
#endif

int main()
{
  const worm::core::Statement statement{"SELECT 1"};
  return statement.sql == "SELECT 1" ? 0 : 1;
}
