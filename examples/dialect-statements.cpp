#include <core/query/criteria.hpp>
#include <core/query/query-builder.hpp>
#include <core/query/sql-builder.hpp>

#include <cstdint>
#include <iostream>
#include <string_view>

namespace
{
  bool showStatement(std::string_view name, const worm::core::SqlBuilder& sqlBuilder)
  {
    const worm::core::QueryBuilder queryBuilder{sqlBuilder};
    worm::core::Criteria criteria;
    criteria.where(worm::core::Filter{worm::core::Predicate::equal("u.active", true)})
      .orderBy(worm::core::Ordering{"u.id"})
      .paginate(worm::core::Pagination{10, 20});

    const worm::core::Statement statement = queryBuilder.selectAll({"users", "u"}, criteria);
    std::cout << name << ": " << statement.sql << "\n";
    return statement.parameters.size() == 3;
  }
} // namespace

int main()
{
  const bool postgres = showStatement("PostgreSQL", worm::core::PgBuilder{});
  const bool mysql = showStatement("MySQL", worm::core::MySqlBuilder{});
  const bool sqlite = showStatement("SQLite", worm::core::SqliteBuilder{});
  const bool sqlServer = showStatement("SQL Server", worm::core::SqlServerBuilder{});
  return postgres && mysql && sqlite && sqlServer ? 0 : 1;
}
