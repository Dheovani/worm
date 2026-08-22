#include <core/query/sql-builder.hpp>

#include <core/query/predicate.hpp>
#include <errors/sql-build-exception.hpp>
#include <utils/dependency-injection.hpp>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
  void setEnvironment(const char* key, const char* value)
  {
#ifdef _WIN32
    _putenv_s(key, value);
#else
    setenv(key, value, 1);
#endif
  }

  void unsetEnvironment(const char* key)
  {
#ifdef _WIN32
    _putenv_s(key, "");
#else
    unsetenv(key);
#endif
  }

  int assertFactoryReturnsBuilder(std::string_view databaseType)
  {
    const std::string database{databaseType};
    setEnvironment("DATABASE_TYPE", database.c_str());

    const auto& builder = worm::DependencyInjector<worm::core::SqlBuilder>::get();

    if (databaseType == "postgresql" && dynamic_cast<const worm::core::PgBuilder*>(&builder) == nullptr) {
      std::cerr << "SqlBuilder injector did not return PgBuilder.\n";
      return 1;
    }

    if (databaseType == "mysql" && dynamic_cast<const worm::core::MySqlBuilder*>(&builder) == nullptr) {
      std::cerr << "SqlBuilder injector did not return MySqlBuilder.\n";
      return 1;
    }

    if (databaseType == "sqlite" && dynamic_cast<const worm::core::SqliteBuilder*>(&builder) == nullptr) {
      std::cerr << "SqlBuilder injector did not return SqliteBuilder.\n";
      return 1;
    }

    if (databaseType == "mssql" && dynamic_cast<const worm::core::SqlServerBuilder*>(&builder) == nullptr) {
      std::cerr << "SqlBuilder injector did not return SqlServerBuilder.\n";
      return 1;
    }

    return 0;
  }
} // namespace

int main()
{
  using worm::core::Aggregate;
  using worm::core::Comparison;
  using worm::core::Criteria;
  using worm::core::Field;
  using worm::core::Filter;
  using worm::core::Grouping;
  using worm::core::Join;
  using worm::core::MySqlBuilder;
  using worm::core::OrderDirection;
  using worm::core::Ordering;
  using worm::core::PgBuilder;
  using worm::core::Predicate;
  using worm::core::Relation;
  using worm::core::Source;
  using worm::core::SqliteBuilder;
  using worm::core::SqlServerBuilder;

  const PgBuilder pgBuilder;
  const MySqlBuilder mySqlBuilder;
  const SqliteBuilder sqliteBuilder;
  const SqlServerBuilder sqlServerBuilder;

  const Source users{"users", "u"};
  const Source orders{"orders", "o"};
  const std::vector<Field> fields{
    Field{"id", users},
    Field{"total", orders},
  };
  const std::vector<Relation> relations{
    Relation{Join::Inner, users, orders, Predicate::compare("u.id", Comparison::Equal, std::int64_t{7})},
  };
  const Filter filter{Predicate::equal("u.active", true)};
  const std::vector<Ordering> ordering{
    Ordering{"o.total", OrderDirection::Descending},
  };
  const worm::core::Statement select = pgBuilder.select(fields, users, relations, filter, ordering);

  if (select.sql != "select u.id,o.total from users u inner join orders o on (u.id = $1)"
                    " where u.active = $2 order by o.total desc") {
    std::cerr << "Select builder did not render joined source, filter, or ordering correctly.\n";
    return 1;
  }

  if (select.parameters != std::vector<worm::core::Parameter>{std::int64_t{7}, true}) {
    std::cerr << "Select builder did not preserve relation and filter parameters.\n";
    return 1;
  }

  const std::vector<Field> projections{
    Field{"id", users, "user_id"},
    Field{"total", orders, Aggregate::Sum, "total_spent"},
    Field{"total", orders, Aggregate::Average, "average_spent"},
    Field{"total", orders, Aggregate::Minimum, "minimum_spent"},
    Field{"total", orders, Aggregate::Maximum, "maximum_spent"},
    Field{"*", users, Aggregate::Count, "row_count"},
  };
  const Filter having{Predicate::compare("sum(o.total)", Comparison::Greater, std::int64_t{100})};
  const worm::core::Statement projected =
    pgBuilder.select(projections, users, relations, filter, ordering, std::nullopt, {Grouping{"u.id"}}, having);
  if (projected.sql != "select u.id as user_id,sum(o.total) as total_spent,avg(o.total) as average_spent,"
                       "min(o.total) as minimum_spent,max(o.total) as maximum_spent,count(*) as row_count"
                       " from users u inner join orders o on (u.id = $1) where u.active = $2 group by u.id"
                       " having sum(o.total) > $3 order by o.total desc" ||
      projected.parameters != std::vector<worm::core::Parameter>{std::int64_t{7}, true, std::int64_t{100}}) {
    std::cerr << "Select builder did not render aliased, grouped, and aggregate projections correctly.\n";
    return 1;
  }

  Criteria aggregateCriteria;
  aggregateCriteria
    .addRelation(Relation{Join::Inner, users, orders, Predicate::compare("u.id", Comparison::Equal, std::int64_t{7})})
    .where(Filter{Predicate::equal("u.active", true)})
    .groupBy(Grouping{"u.id"})
    .having(Filter{Predicate::compare("sum(o.total)", Comparison::Greater, std::int64_t{100})})
    .orderBy(Ordering{"o.total", OrderDirection::Descending});

  const worm::core::Statement criteriaProjection = pgBuilder.select(projections, users, aggregateCriteria);
  if (criteriaProjection.sql != projected.sql || criteriaProjection.parameters != projected.parameters) {
    std::cerr << "Select builder did not render Criteria-based grouped projections consistently.\n";
    return 1;
  }

  try {
    static_cast<void>(pgBuilder.select(projections, users, relations, filter, ordering));
    std::cerr << "Select builder accepted mixed aggregate and non-aggregate projections without grouping.\n";
    return 1;
  } catch (const worm::SqlBuildException&) {}

  try {
    static_cast<void>(pgBuilder.select(
      {Field{"*", users, Aggregate::Count, "row_count"}}, users, {}, std::nullopt, {}, std::nullopt, {}, having));
    std::cerr << "Select builder accepted HAVING without GROUP BY.\n";
    return 1;
  } catch (const worm::SqlBuildException&) {}

  try {
    static_cast<void>(pgBuilder.select(
      {Field{"*", users, Aggregate::Count, "row_count"}}, users, {}, std::nullopt, {}, std::nullopt, {Grouping{""}}));
    std::cerr << "Select builder accepted an empty GROUP BY column.\n";
    return 1;
  } catch (const worm::SqlBuildException&) {}

  try {
    static_cast<void>(pgBuilder.select({}, users, relations, filter, ordering));
    std::cerr << "Select builder accepted an empty projection list.\n";
    return 1;
  } catch (const worm::SqlBuildException&) {}

  try {
    static_cast<void>(pgBuilder.select({Field{"*", users, Aggregate::Sum}}, users, {}, std::nullopt, {}));
    std::cerr << "Select builder accepted a wildcard for an aggregate other than COUNT.\n";
    return 1;
  } catch (const worm::SqlBuildException&) {}

  try {
    static_cast<void>(pgBuilder.select({Field{"id", users, ""}}, users, {}, std::nullopt, {}));
    std::cerr << "Select builder accepted an empty projection alias.\n";
    return 1;
  } catch (const worm::SqlBuildException&) {}

  const worm::core::Statement mySqlSelect = mySqlBuilder.select(fields, users, relations, filter, ordering);
  const worm::core::Statement sqliteSelect = sqliteBuilder.select(fields, users, relations, filter, ordering);

  if (mySqlSelect.sql != "select u.id,o.total from users u inner join orders o on (u.id = ?)"
                         " where u.active = ? order by o.total desc" ||
      sqliteSelect.sql != "select u.id,o.total from users u inner join orders o on (u.id = ?)"
                          " where u.active = ? order by o.total desc") {
    std::cerr << "Question mark dialect placeholders were rendered incorrectly.\n";
    return 1;
  }

  const Source payments{"payments", "p"};
  const std::vector<Relation> multipleRelations{
    Relation{Join::Inner, users, orders, Predicate::compare("u.id", Comparison::Equal, std::int64_t{7})},
    Relation{Join::Left, orders, payments, Predicate::compare("o.id", Comparison::Equal, std::int64_t{9})},
  };

  const worm::core::Statement multiJoinSelect = pgBuilder.select(fields, users, multipleRelations, filter, ordering);
  if (multiJoinSelect.sql != "select u.id,o.total from users u inner join orders o on (u.id = $1)"
                             " left join payments p on (o.id = $2) where u.active = $3 order by o.total desc") {
    std::cerr << "Select builder did not separate multiple joins or number parameters correctly.\n";
    return 1;
  }

  if (multiJoinSelect.parameters != std::vector<worm::core::Parameter>{std::int64_t{7}, std::int64_t{9}, true}) {
    std::cerr << "Select builder did not preserve multiple relation parameters before filter parameters.\n";
    return 1;
  }

  const worm::core::Pagination pagination{10, 20};
  const worm::core::Statement pgPaginated = pgBuilder.select(fields, users, relations, filter, ordering, pagination);
  if (pgPaginated.sql != "select u.id,o.total from users u inner join orders o on (u.id = $1)"
                         " where u.active = $2 order by o.total desc limit $3 offset $4" ||
      pgPaginated.parameters !=
        std::vector<worm::core::Parameter>{std::int64_t{7}, true, std::int64_t{10}, std::int64_t{20}}) {
    std::cerr << "PostgreSQL pagination did not preserve SQL or parameter ordering.\n";
    return 1;
  }

  const worm::core::Statement mySqlPaginated =
    mySqlBuilder.select(fields, users, relations, filter, ordering, pagination);
  if (mySqlPaginated.sql != "select u.id,o.total from users u inner join orders o on (u.id = ?)"
                            " where u.active = ? order by o.total desc limit ? offset ?" ||
      mySqlPaginated.parameters != pgPaginated.parameters) {
    std::cerr << "MySQL pagination did not preserve SQL or parameter ordering.\n";
    return 1;
  }

  const worm::core::Statement sqlServerPaginated =
    sqlServerBuilder.select(fields, users, relations, filter, ordering, pagination);
  if (sqlServerPaginated.sql != "select u.id,o.total from users u inner join orders o on (u.id = ?)"
                                " where u.active = ? order by o.total desc offset ? rows fetch next ? rows only" ||
      sqlServerPaginated.parameters !=
        std::vector<worm::core::Parameter>{std::int64_t{7}, true, std::int64_t{20}, std::int64_t{10}}) {
    std::cerr << "SQL Server pagination did not preserve SQL or parameter ordering.\n";
    return 1;
  }

  try {
    static_cast<void>(sqlServerBuilder.select(fields, users, relations, filter, {}, pagination));
    std::cerr << "SQL Server pagination accepted a query without ordering.\n";
    return 1;
  } catch (const worm::SqlBuildException&) {}

  const std::vector<std::pair<std::string, worm::core::Parameter>> insertColumns{
    {"name", std::string{"Ada"}},
    {"active", true},
  };

  const worm::core::Statement pgInsert = pgBuilder.insert(users, insertColumns);
  const worm::core::Statement mySqlInsert = mySqlBuilder.insert(users, insertColumns);
  const worm::core::Statement sqliteInsert = sqliteBuilder.insert(users, insertColumns);
  if (pgInsert.sql != "insert into users(name,active) values ($1,$2)" ||
      mySqlInsert.sql != "insert into users(name,active) values (?,?)" ||
      sqliteInsert.sql != "insert into users(name,active) values (?,?)") {
    std::cerr << "Insert builder did not render placeholders correctly.\n";
    return 1;
  }

  if (pgInsert.parameters != std::vector<worm::core::Parameter>{std::string{"Ada"}, true}) {
    std::cerr << "Insert builder did not preserve column parameters.\n";
    return 1;
  }

  const Source archivedUsers{"archived_users"};
  const std::vector<std::string> targetColumns{
    "id",
    "total",
  };

  const worm::core::Statement rawInsertFromSelect = pgBuilder.insertFromSelect(archivedUsers, targetColumns, select);
  if (rawInsertFromSelect.sql != "insert into archived_users(id,total) "
                                 "select u.id,o.total from users u inner join orders o on (u.id = $1)"
                                 " where u.active = $2 order by o.total desc") {
    std::cerr << "Insert from raw select query was not rendered correctly.\n";
    return 1;
  }

  if (rawInsertFromSelect.parameters != select.parameters) {
    std::cerr << "Insert from raw select query did not preserve source parameters.\n";
    return 1;
  }

  const worm::core::Statement structuredInsertFromSelect =
    pgBuilder.insertFromSelect(archivedUsers, targetColumns, fields, users, relations, filter, ordering);
  if (structuredInsertFromSelect.sql != "insert into archived_users(id,total) "
                                        "select u.id,o.total from users u inner join orders o on (u.id = $1)"
                                        " where u.active = $2 order by o.total desc") {
    std::cerr << "Insert from structured select query was not rendered correctly.\n";
    return 1;
  }

  if (structuredInsertFromSelect.parameters != select.parameters) {
    std::cerr << "Insert from structured select query did not preserve source parameters.\n";
    return 1;
  }

  const worm::core::Statement pgUpdate = pgBuilder.update(users, insertColumns, filter);
  const worm::core::Statement mySqlUpdate = mySqlBuilder.update(users, insertColumns, filter);
  const worm::core::Statement sqliteUpdate = sqliteBuilder.update(users, insertColumns, filter);
  const worm::core::Statement sqlServerUpdate = sqlServerBuilder.update(users, insertColumns, filter);
  if (pgUpdate.sql != "update users u set name = $1,active = $2 where u.active = $3" ||
      mySqlUpdate.sql != "update users u set name = ?,active = ? where u.active = ?" ||
      sqliteUpdate.sql != "update users as u set name = ?,active = ? where u.active = ?" ||
      sqlServerUpdate.sql != "update u set name = ?,active = ? from users u where u.active = ?") {
    std::cerr << "Update builder did not render placeholders or filter correctly.\n";
    return 1;
  }

  if (pgUpdate.parameters != std::vector<worm::core::Parameter>{std::string{"Ada"}, true, true}) {
    std::cerr << "Update builder did not preserve column parameters before filter parameters.\n";
    return 1;
  }

  bool emptyUpdateFailed = false;
  try {
    static_cast<void>(pgBuilder.update(users, {}, filter));
  } catch (const worm::SqlBuildException&) {
    emptyUpdateFailed = true;
  }

  if (!emptyUpdateFailed) {
    std::cerr << "Update builder accepted an update without columns.\n";
    return 1;
  }

  const worm::core::Statement pgDelete = pgBuilder.delete_(users, filter);
  const worm::core::Statement mySqlDelete = mySqlBuilder.delete_(users, filter);
  const worm::core::Statement sqliteDelete = sqliteBuilder.delete_(users, filter);
  const worm::core::Statement sqlServerDelete = sqlServerBuilder.delete_(users, filter);
  if (pgDelete.sql != "delete from users u where u.active = $1" ||
      mySqlDelete.sql != "delete from users u where u.active = ?" ||
      sqliteDelete.sql != "delete from users as u where u.active = ?" ||
      sqlServerDelete.sql != "delete u from users u where u.active = ?") {
    std::cerr << "Delete builder did not render placeholders or filter correctly.\n";
    return 1;
  }

  if (pgDelete.parameters != std::vector<worm::core::Parameter>{true}) {
    std::cerr << "Delete builder did not preserve filter parameters.\n";
    return 1;
  }

  const worm::core::Table schemaUsers{worm::core::Schema{"public"}, "users"};
  const worm::core::Column schemaId{
    worm::reflection::FieldMetadata{.columnName = "id", .generated = true, .nullable = false}, schemaUsers};
  const worm::core::Column schemaEmail{
    worm::reflection::FieldMetadata{.columnName = "email", .nullable = false}, schemaUsers};
  const worm::core::TableMetadata usersMetadata{
    schemaUsers,
    {
      worm::core::ColumnMetadata{schemaId, {.kind = worm::core::ColumnTypeKind::Int64}},
      worm::core::ColumnMetadata{schemaEmail, {.kind = worm::core::ColumnTypeKind::String, .length = std::size_t{120}}},
    },
    worm::core::PrimaryKey{"pk_users", {schemaId}},
    {worm::core::Index{"idx_users_email", {{schemaEmail}}, true}},
  };

  const auto pgCreate = pgBuilder.create(usersMetadata);
  const auto mySqlCreate = mySqlBuilder.create(usersMetadata);
  const auto sqliteCreate = sqliteBuilder.create(usersMetadata);
  const auto sqlServerCreate = sqlServerBuilder.create(usersMetadata);
  if (pgCreate.size() != 2 ||
      pgCreate[0].sql != "create table \"public\".\"users\" (\"id\" bigint generated by default as identity not "
                         "null,\"email\" varchar(120) not null,constraint \"pk_users\" primary key (\"id\"))" ||
      pgCreate[1].sql != "create unique index \"idx_users_email\" on \"public\".\"users\" (\"email\" asc)") {
    std::cerr << "PostgreSQL CREATE TABLE generation failed.\n";
    return 1;
  }
  if (mySqlCreate[0].sql.find("`id` bigint auto_increment not null") == std::string::npos ||
      sqliteCreate[0].sql.find("\"id\" integer primary key autoincrement") == std::string::npos ||
      sqlServerCreate[0].sql.find("[id] bigint identity(1,1) not null") == std::string::npos) {
    std::cerr << "Dialect-specific generated-column DDL was not rendered correctly.\n";
    return 1;
  }

  try {
    const worm::core::TableMetadata invalidMetadata{
      schemaUsers, {worm::core::ColumnMetadata{schemaId, {.kind = worm::core::ColumnTypeKind::Unknown}}}};
    static_cast<void>(pgBuilder.create(invalidMetadata));
    std::cerr << "CREATE TABLE accepted a column with an unknown type.\n";
    return 1;
  } catch (const worm::SqlBuildException&) {}

  int result = 0;
  try {
    result = assertFactoryReturnsBuilder("postgresql");
    if (result == 0) {
      result = assertFactoryReturnsBuilder("mysql");
    }
    if (result == 0) {
      result = assertFactoryReturnsBuilder("sqlite");
    }
    if (result == 0) {
      result = assertFactoryReturnsBuilder("mssql");
    }
  } catch (const std::exception& error) {
    std::cerr << "SqlBuilder factory test failed: " << error.what() << '\n';
    result = 1;
  }

  unsetEnvironment("DATABASE_TYPE");

  return result;
}
