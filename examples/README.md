# Worm examples

These examples are complete, executable programs for the primary Worm APIs. They use disposable SQLite databases where persistence is required and remove those files after execution.

| Example | Main concepts |
| --- | --- |
| [`sqlite-quick-start.cpp`](sqlite-quick-start.cpp) | Entity mapping, CRUD, parameterized lookup, commit, and normalized errors |
| [`sqlite-queries.cpp`](sqlite-queries.cpp) | `Criteria`, filters, ordering, pagination, inspectable SQL, and bound parameters |
| [`sqlite-relationships.cpp`](sqlite-relationships.cpp) | One-to-one, one-to-many, many-to-many, and explicit relationship joins |
| [`reflection-and-snapshots.cpp`](reflection-and-snapshots.cpp) | Field visitation, lookup by member or column, snapshots, ignored fields, and dirty detection |
| [`result-hydration.cpp`](result-hydration.cpp) | Result rows, typed hydration, `NULL`, empty strings, enums, dates, exact decimals, and binary values |
| [`sqlite-session.cpp`](sqlite-session.cpp) | `Session`, repository reuse, identity map, snapshots, and partial updates |
| [`sqlite-transactions.cpp`](sqlite-transactions.cpp) | Explicit commit, explicit rollback, scope rollback, and persistence-context boundaries |
| [`schema-migration-plan.cpp`](schema-migration-plan.cpp) | Reflected schema metadata, schema differences, reviewable migration plans, and dialect-specific DDL |
| [`dialect-statements.cpp`](dialect-statements.cpp) | Inspectable SQL and placeholder differences for PostgreSQL, MySQL, SQLite, and SQL Server |
| [`sqlite-view.cpp`](sqlite-view.cpp) | Immutable view metadata, read-only repository access, and typed hydration from a database view |

## Build

Enable examples and the SQLite driver when configuring Worm:

```powershell
cmake -S . -B build/examples `
  -DBUILD_TESTING=ON `
  -DWORM_BUILD_EXAMPLES=ON `
  -DWORM_ENABLE_POSTGRESQL=OFF `
  -DWORM_ENABLE_MYSQL=OFF `
  -DWORM_ENABLE_SQLITE=ON `
  -DWORM_ENABLE_MSSQL=OFF `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"

cmake --build build/examples --config Debug --parallel
ctest --test-dir build/examples -C Debug -L examples --output-on-failure
```

Each source can also be read independently. The `sqlite-example-support.hpp` file only creates and removes disposable SQLite databases; it is not part of Worm's public API.
