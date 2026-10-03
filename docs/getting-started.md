# Getting started

This guide shows the smallest complete flow currently supported by Worm: configure a SQLite build, map an entity, insert, query, update, delete records, and control a transaction. The matching buildable example lives in [`examples/sqlite-quick-start.cpp`](../examples/sqlite-quick-start.cpp). The complete [examples index](../examples/README.md) covers the principal reflection, hydration, persistence, query, relationship, transaction, schema, and migration-planning APIs.

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

Install the selected build configuration to a prefix. With a multi-configuration generator, install both Debug and Release so each consumer configuration resolves the matching library:

```powershell
cmake --install build --config Debug --prefix C:/worm
cmake --install build --config Release --prefix C:/worm
```

Configure consumers with the same dependency toolchain and make the installation prefix discoverable:

```cmake
find_package(Worm CONFIG REQUIRED)

add_executable(my_application main.cpp)
target_compile_features(my_application PRIVATE cxx_std_20)
target_link_libraries(my_application PRIVATE Worm::Core Worm::Connection)
```

```powershell
cmake -S . -B build `
  -DCMAKE_PREFIX_PATH=C:/worm `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
```

The installed package exports `Worm::Errors`, `Worm::Reflection`, `Worm::Utils`, `Worm::Core`, and `Worm::Connection`. Its configuration resolves only the database dependencies enabled when Worm was built. Debug libraries use a `d` postfix, and MinSizeRel or RelWithDebInfo consumers fall back to the installed Release libraries.

For source-tree integration, the repository can still be included as a subdirectory:

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

  static constexpr worm::core::PrimaryKey primaryKey() noexcept
  {
    return worm::core::PrimaryKey{"pk_users", {worm::core::Column{"id", table()}}};
  }

  static constexpr auto reflect() noexcept
  {
    return std::tuple{
      worm::reflection::field("id", &User::id),
      worm::reflection::field("name", &User::name),
      worm::reflection::field("email", &User::email)};
  }
};
```

The name passed to `field()` is also the column name unless
`FieldMetadata::columnName` is set. `PrimaryKey` is declared separately from field metadata so schema constraints can be represented independently from C++ member descriptors. Fields marked as `ignored` are not persisted.
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

Use `worm::core::Decimal` for fixed-precision decimal fields and parameters so Worm never converts them through `double`. Use `worm::core::Binary` for owned byte sequences, including embedded null bytes. Both types participate in parameter encoding, result hydration, statement hashing, and native driver bindings. SQLite may still normalize decimal values according to its NUMERIC affinity because the database engine itself does not provide an exact fixed-decimal storage class.

Native enum definitions can be declared in generator manifests with `"type": "enum"`, `"values": [...]`, and a PostgreSQL `"enumName"`. PostgreSQL and MySQL preserve their native enum facilities; SQLite and SQL Server reject this schema request because they do not provide an equivalent native enum type.

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

Relationships are declared as explicit metadata and can be included in `Criteria` to generate joins. Worm supports one-to-one, one-to-many, and many-to-many descriptors. Each descriptor also declares a `RelationshipLoadStrategy`, making `Explicit`, `Eager`, or `Lazy` a visible model choice even though automatic eager/lazy loading is not implemented yet. Cascades and orphan removal are also explicit metadata and default to disabled:

```cpp
const auto userProfile = worm::core::oneToOne<User, Profile>("profile", "id", "user_id");
const worm::core::CascadePolicy postCascade{.persist = true, .update = true, .remove = true};
const auto userPosts = worm::core::oneToMany<User, Post>(
  "posts",
  "id",
  "user_id",
  worm::core::Join::Left,
  worm::core::RelationshipLoadStrategy::Eager,
  postCascade,
  true);
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

The descriptors only produce relationship-aware join metadata today. Hydrating object graphs and executing eager/lazy loaders are separate features still tracked in the roadmap. Cascade metadata and orphan removal do not perform automatic mutations yet; they make the intended ownership policy visible for future persistence flows.

## N+1 diagnostics

`NPlusOneDetector` can be used in tests, development tooling, or repository wrappers to inspect executed statements and detect repeated parameterized `SELECT` shapes with different parameter sets:

```cpp
worm::utils::NPlusOneDetector detector;
detector.record({"select * from posts where user_id = ?", {std::int64_t{1}}});
detector.record({"select * from posts where user_id = ?", {std::int64_t{2}}});

for (const worm::utils::NPlusOneWarning& warning : detector.warnings()) {
  // warning.sql contains the parameterized SQL, never the concrete parameter values.
}
```

The detector is intentionally opt-in. It does not collect statements globally, does not block execution, and does not inspect parameter values in diagnostics. Repeated parameterized `SELECT` statements are a strong signal that a relationship query may be running once per parent row; use `Criteria::include()` and explicit joins when the relationship should be loaded in one query.

The optional CLI exposes the same opt-in analysis for captured SQL without connecting to a database or executing the statements. Pass one observed query through `--query`, or place multiple semicolon-separated `SELECT` statements in a file and use `--file`:

```bash
worm n-plus-one --query "SELECT * FROM posts WHERE user_id = 42"
worm n-plus-one --file query-log.sql --max-executions 2
```

Exactly one input source is required. The default maximum is one execution of each normalized query pattern; `--max-executions` changes that allowed count. Reports contain normalized SQL and aggregate counts but never concrete literal values. A detected pattern exits with code `2`, while invalid or non-read-only input exits with code `1`.

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

Repository, mapping, and hydration failures preserve the available operation and model context. Diagnostics identify the entity table or view and, when applicable, the reflected field and mapped column; SQL parameter values and connection credentials are not included.

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

Worm does not keep a reusable prepared statement cache. Each `Statement` is prepared and executed inside the driver call; `WormDriverBenchmarks` measures that full round-trip because preparation is not exposed as a separate public operation. The current decision is to avoid cache invalidation, native-handle lifetime, and reconnect complexity until application measurements identify statement preparation as a material bottleneck. Worm also has no connection pool: connection opening/closing is measurable with the same benchmark, while applications should continue using one `Session` and one `Client` per concurrent workflow until production latency and concurrency requirements justify explicit pool ownership and limits.

## Current limitations

The canonical [current limitations](limitations.md) list covers stability, distribution, drivers, persistence, relationships, migrations, performance, and diagnostics. Consult it before adopting Worm or upgrading between revisions; migration-specific details remain in the [migration guide](migrations.md).
