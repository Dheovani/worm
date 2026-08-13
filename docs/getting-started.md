# Getting started

This guide shows the smallest complete flow currently supported by Worm:
configure a SQLite build, map an entity, insert, query, update, delete records,
and control a transaction. The matching buildable example lives in
[`examples/sqlite-quick-start.cpp`](../examples/sqlite-quick-start.cpp).

## API status

Worm is still under development and does not provide binary stability or
compatibility guarantees between versions. Use it for experimentation and
contribution, not for production data.

## Build the quick start

You need CMake 3.20, a C++20 compiler, and vcpkg. With `VCPKG_ROOT` configured,
a minimal build can use only the SQLite feature from the manifest:

```powershell
cmake -S . -B build/quick-start `
  -DBUILD_TESTING=OFF `
  -DWORM_BUILD_EXAMPLES=ON `
  -DWORM_ENABLE_POSTGRESQL=OFF `
  -DWORM_ENABLE_MYSQL=OFF `
  -DWORM_ENABLE_SQLITE=ON `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"

cmake --build build/quick-start --config Debug
./build/quick-start/examples/Debug/WormSqliteQuickStart.exe
```

With single-configuration generators such as Ninja, the executable is usually
placed directly under `build/quick-start/examples/`.

## Consume Worm with CMake

Until install rules and `find_package(Worm)` are available, include the
repository as a subdirectory:

```cmake
set(WORM_ENABLE_POSTGRESQL OFF CACHE BOOL "" FORCE)
set(WORM_ENABLE_MYSQL OFF CACHE BOOL "" FORCE)
set(WORM_ENABLE_SQLITE ON CACHE BOOL "" FORCE)
set(WORM_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

add_subdirectory(external/worm)

add_executable(my_application main.cpp)
target_compile_features(my_application PRIVATE cxx_std_20)
target_link_libraries(my_application PRIVATE Worm::Core Worm::Connection)
```

## Define an entity

A persistable entity must provide `table()`, `reflect()`, and exactly one
persistent primary key:

```cpp
struct User
{
  std::int64_t id{};
  std::string name;
  std::optional<std::string> email;

  static constexpr worm::core::Table table() noexcept
  {
    return worm::core::Table{"users"};
  }

  static constexpr auto reflect() noexcept
  {
    return std::tuple{
      worm::reflection::field("id", &User::id, {.primaryKey = true}),
      worm::reflection::field("name", &User::name),
      worm::reflection::field("email", &User::email)};
  }
};
```

The name passed to `field()` is also the column name unless
`FieldMetadata::columnName` is set. Fields marked as `ignored` are not persisted.
The current portable flow uses application-provided keys. Keys marked as
`generated` require `INSERT` to return a row with the generated value, which is
not yet complete across all drivers.

## Create ORM objects

Explicit injection is the recommended path because it keeps connection ownership
and lifetime visible:

```cpp
const worm::connection::ConnectionConfig config{
  .dbname = "application.db",
  .timeoutConfig = {
    .connectionTimeout = std::chrono::seconds{5},
    .queryTimeout = std::chrono::seconds{30},
  },
};

const auto client = std::make_shared<worm::connection::SqliteClient>(config);
const worm::core::SqliteBuilder sqlBuilder;
const worm::core::QueryBuilder queryBuilder{sqlBuilder};
const auto registry = std::make_shared<worm::core::Registry>();
const worm::core::Repository<User> users{client, queryBuilder, registry};
```

Worm does not create tables yet. The schema must exist before using the
repository:

```sql
CREATE TABLE users (
  id INTEGER PRIMARY KEY,
  name TEXT NOT NULL,
  email TEXT NULL
);
```

## CRUD

```cpp
const std::shared_ptr<User> created = users.insert({
  .id = 1,
  .name = "Ada",
  .email = "ada@example.com",
});

const std::shared_ptr<User> found = users.find(std::int64_t{1});

found->name = "Ada Lovelace";
const std::uint64_t affected = users.update(found->id, *found);

users.delete_(found->id);
```

`find()` returns `nullptr` when no row is found. Repeated `find(id)` calls in the
same `Registry` reuse the registered `shared_ptr`. When a snapshot exists for
the entity, `update()` sends only changed fields and returns the number of
affected rows.

## Parameterized queries

Values should never be concatenated into SQL. Use the builders:

```cpp
worm::core::Criteria criteria;
criteria.where(worm::core::Filter{
  worm::core::Predicate::equal("users.name", std::string{"Ada Lovelace"})});

const worm::core::Statement statement = queryBuilder.selectAll({User::table().name()}, criteria);

const std::vector<std::shared_ptr<User>> result = users.findAll(statement);
```

`Criteria` is only an explicit query envelope for relations, filters, grouping, ordering, having, and pagination. The generated `Statement` remains inspectable and keeps parameters separate from SQL text. The overloads that receive `Statement` are the controlled escape hatch for manual SQL, but they still accept only the operation that matches the repository method and keep parameters separate from the SQL text.

Use `Field` entries to select specific columns, assign result aliases, or request the supported `COUNT`, `SUM`, `AVG`, `MIN`, and `MAX` aggregates. When a query mixes aggregate and non-aggregate projections, pass explicit `Grouping` entries and, when needed, a `HAVING` filter:

```cpp
const worm::core::Source usersSource{User::table().name(), "u"};
worm::core::Criteria aggregateCriteria;
aggregateCriteria.groupBy(worm::core::Grouping{"u.id"})
  .having(worm::core::Filter{
    worm::core::Predicate::compare("count(*)", worm::core::Comparison::Greater, std::int64_t{0})});

const worm::core::Statement countUsers = queryBuilder.select(
  {
    worm::core::Field{"id", usersSource, "user_id"},
    worm::core::Field{"*", usersSource, worm::core::Aggregate::Count, "user_count"},
  },
  usersSource,
  aggregateCriteria);
```

`HAVING` requires `GROUP BY`. Worm also rejects mixed aggregate and non-aggregate projections when no grouping is provided, because the resulting SQL would be ambiguous or invalid on stricter databases.

Pass `Pagination{limit, offset}` to a select operation to paginate in the database. The limit and offset remain bound parameters instead of being interpolated into SQL:

```cpp
const worm::core::Statement page = queryBuilder.selectAll(
  {User::table().name()},
  {},
  std::nullopt,
  {worm::core::Ordering{"users.id"}},
  worm::core::Pagination{25, 50});
```

SQL Server requires an ordering when pagination is present. The other supported dialects render `LIMIT` and `OFFSET`. `worm::core::Paginator` is available only when an already loaded `ResultSet` must be divided in memory; it does not reduce the number of rows fetched from the database.

## Relationships

Relationships are declared as explicit metadata and can be included in `Criteria` to generate joins. Worm supports one-to-one, one-to-many, and many-to-many descriptors. Each descriptor also declares a `RelationshipLoadStrategy`, making `Explicit`, `Eager`, or `Lazy` a visible model choice even though automatic eager/lazy loading is not implemented yet:

```cpp
const auto userProfile = worm::core::oneToOne<User, Profile>("profile", "id", "user_id");
const auto userPosts = worm::core::oneToMany<User, Post>(
  "posts",
  "id",
  "user_id",
  worm::core::Join::Left,
  worm::core::RelationshipLoadStrategy::Eager);
const auto userRoles = worm::core::manyToMany<User, Role>(
  "roles",
  "user_roles",
  "id",
  "user_id",
  "role_id",
  "id",
  worm::core::Join::Left,
  worm::core::RelationshipLoadStrategy::Lazy);

worm::core::Criteria criteria;
criteria.include(userProfile, "u", "p")
  .include(userPosts, "u", "po")
  .include(userRoles, "u", "ur", "r");

const worm::core::Statement statement = queryBuilder.select(
  {
    worm::core::Field{"id", {"users", "u"}},
    worm::core::Field{"id", {"profiles", "p"}, "profile_id"},
  },
  {"users", "u"},
  criteria);
```

The descriptors only produce relationship-aware join metadata today. Hydrating object graphs, executing eager/lazy loaders, detecting N+1 queries, and defining cascades are separate features still tracked in the roadmap.

## Transactions

A transaction must be finalized explicitly. If it leaves scope while still
active, its destructor attempts a rollback:

```cpp
{
  auto transaction = client->beginTransaction();
  static_cast<void>(users.insert(User{.id = 2, .name = "Grace"}));
  transaction.commit();
}
```

After rollback, discard or recreate the `Registry`: the database reverts the
data, but the identity map does not yet reconcile entities inserted or modified
inside the transaction.

## Errors

Public errors derive from `worm::WormException`. Catch specific types when
recovery is possible and the base type at the application boundary:

```cpp
try {
  const std::shared_ptr<User> user = users.find(std::int64_t{1});
} catch (const worm::QueryExecutionException& error) {
  // Failure reported by the database or driver.
} catch (const worm::WormException& error) {
  // Another normalized ORM error.
}
```

## Persistence context and lifetime

`Session` centralizes the client, the identity map, and repositories. The
database type is read from `DATABASE_TYPE`, documented in
[`.env.example`](../.env.example):

```cpp
const worm::core::Session context(config);
const auto& users = context.repository<User>();
```

The context and its associated objects belong to the thread that created them.
Access from another thread raises `ConcurrentAccessException`; use a separate
context and connection for each concurrent workflow. An active transaction must
also be committed or rolled back on the owner thread. If it leaves scope while
still active, its destructor attempts a rollback.

`Repository` keeps shared ownership of the client and registry it receives.
Entities returned as `shared_ptr` may outlive the context, but the shared pointer
does not synchronize modifications made to the entity itself.

The `SELECT` result cache is opt-in through `QUERY_CACHE_ENABLED=true`. The key
contains the SQL and its parameters; mutations and transactions invalidate the
cache. Keep it disabled when the same database can be changed by other processes
and the application must observe those changes immediately.

Timeouts are configured through `ConnectionConfig::timeoutConfig` or the
`CONNECTION_TIMEOUT_MS` and `QUERY_TIMEOUT_MS` environment variables. Worm only
applies behavior that each driver supports predictably: PostgreSQL uses
`connect_timeout` and `statement_timeout`, MySQL configures native connection
and I/O timeouts, SQLite uses `busy_timeout` for lock waits, and SQL Server uses
ODBC login and statement attributes. Millisecond values are rounded up when a
driver only accepts seconds.

Worm does not keep a reusable prepared statement cache at the moment. Each
`Statement` is prepared and executed inside the driver call. This avoids a
premature per-connection invalidation policy and will be revisited only after
benchmarks or a real use case. For the same reason, there is no connection pool:
use one `Session` and one `Client` per concurrent workflow.

## Current limitations

- Migrations and schema creation are not implemented yet.
- Database-generated keys do not yet have portable behavior across drivers.
- The SQL Server driver compiles and implements the ODBC contract, but it does
  not yet have a contract test running against a real CI instance.
- Automatic relationship loading, eager/lazy execution, and N+1 detection do not exist yet.
- There is no connection pool or prepared statement cache.
- A single `Client`, `Session`, `Repository`, or `Registry` must not be shared
  across threads; create independent contexts for parallel work.
- Install rules and `find_package(Worm)` still need to be defined.
