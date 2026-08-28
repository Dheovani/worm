# Roadmap

Worm aims to be a small, safe, and predictable C++20 ORM inspired by Doctrine's good ideas without trying to reproduce all of its complexity. Its value for developers should come from a typed API, parameterized SQL, simple CMake integration, and explicit behavior without hidden queries or allocations.

## Project principles

- Safety and correctness come before syntactic convenience.
- The public API should be small, documented, and hard to misuse.
- Features should grow from complete and testable use cases, not isolated abstractions.
- Generated SQL should be inspectable and allow controlled escape hatches for manual SQL.
- Drivers and database dependencies should be optional.
- Errors, limitations, and performance costs should be explicit.
- The project should welcome contributions with respect, documentation, and transparent technical decisions.

## Milestone 0 — Foundation complete

- [x] Organize production code under `src/`.
- [x] Configure CMake with `Worm::*` targets.
- [x] Add a reproducible vcpkg manifest.
- [x] Integrate tests with CTest outside `src/`.
- [x] Standardize style, names, and formatting with `.clang-format` and `AGENTS.md`.
- [x] Cover errors, connections, utilities, and dependency injection.

### Reflection

- [x] Implement typed field descriptors with member pointers.
- [x] Define the C++20 `Reflectable` concept.
- [x] Implement `for_each_field` visitation without allocations or virtual functions.
- [x] Add column metadata, primary key, generated field, and ignored field support.
- [x] Implement lookup by member and column with hash collision handling.
- [x] Create typed snapshots and persistent-field change detection.
- [x] Create separate unit tests for each reflection component.

## Milestone 1 — Minimum usable ORM

This milestone should end with a complete example that creates, persists, queries, changes, and deletes an entity in SQLite without concatenating values into SQL.

### Entity model

- [x] Define entity and table metadata separately from field metadata.
- [x] Reintroduce the entity core on top of the reflection subsystem.
- [x] Validate missing or duplicated primary keys at compile time when possible.
- [x] Define a clear convention for new, persisted, and removed entities.

### SQL and parameters

- [x] Create a `Statement` representation containing SQL and bound parameters.
- [x] Implement initial generation for `SELECT`, `INSERT`, `UPDATE`, `DELETE`, and `INSERT ... SELECT`.
- [x] Migrate builders to return `Statement` with SQL and parameters instead of only `std::string`.
- [x] Create parameterized expressions and `WHERE` and `ORDER BY` clauses.
- [x] Implement grouped logical operators: `AND`, `OR`, and `NOT`.
- [x] Bind parameters in drivers without value interpolation.
- [x] Create a dialect abstraction for placeholders, identifiers, and SQLite/PostgreSQL/MySQL-specific capabilities.
- [x] Allow parameterized manual SQL as a controlled escape hatch.

### Types and hydration

- [x] Define codecs between C++ and SQL types.
- [x] Cover numbers, booleans, text, `std::optional`, and enums.
- [x] Cover dates in codecs between C++ and SQL.
- [x] Correctly distinguish `NULL`, empty string, and default value.
- [x] Hydrate results into reflected entities with diagnostics for invalid columns.
- [x] Handle invalid conversions without silently losing data.

### Persistence

- [x] Implement a typed repository for basic operations.
- [x] Implement an identity map to avoid duplicated instances for the same row.
- [x] Implement a unit-of-work-like flow using snapshots for partial `UPDATE`.
- [x] Implement RAII transactions with explicit commit and rollback.
- [x] Create a consistent error flow across reflection, SQL, and drivers.

## Milestone 2 — Reliable and portable connections

- [x] Add a virtual destructor to the `Client` interface.
- [x] Remove `noexcept` from factories that may fail while connecting.
- [x] Standardize naming and basic handling across the three initial drivers.
- [x] Replace manual ownership in drivers with RAII.
- [x] Separate connection, statement representation, and result into their own types.
- [x] Evaluate reusable prepared statements and per-connection caching.
- [x] Define thread-safety behavior and prevent unsafe concurrent use.
- [x] Implement timeout configuration and cancellation where supported by the driver.
- [x] Make drivers optional so every build does not require every database.
- [x] Create disposable integration tests for PostgreSQL and MySQL.
- [x] Run the same integration contract for all drivers.

## Milestone 3 — Queries and relationships

- [x] Add projections, projection aliases, joins, and basic aggregate projections.
- [x] Add `GROUP BY` and `HAVING` for grouped aggregations.
- [x] Add dialect-aware pagination with bound limit and offset parameters.
- [x] Create a composable criteria API without hiding the resulting SQL.
- [x] Implement one-to-one, one-to-many, and many-to-many relationships.
- [x] Make eager or lazy loading an explicit choice.
- [x] Detect and document N+1 queries.
- [x] Define cascades and orphan removal with conservative defaults.

## Milestone 4 — Schema and migrations

- [x] Represent schema, table, view, column, index, primary key, and foreign key metadata.
- [x] Compare entity metadata with the existing database schema.
- [x] Generate reviewable migrations without executing them automatically.
- [x] Keep migration history, checksum, application, and rollback information.
- [x] Document migration differences and limitations between databases.

### Bidirectional generator

- [x] Define the decision rule between entity-as-source-of-truth, database-as-source-of-truth, or explicit command mode.
- [x] Introspect the database schema and generate C++ entity classes with reflection metadata.
- [ ] Read reflected entities and generate tables, columns, keys, indexes, and relationships in the database.
- [x] Parse columns, keys, indexes, and relationships from declarative manifests and generate them for missing tables.
- [x] Generate missing tables from declarative schema metadata without modifying incompatible existing tables.
- [x] Compare entity and database state to produce a reviewable synchronization plan before any change.
- [x] Generate code and SQL into separate files without applying destructive changes automatically.
- [x] Map C++ and SQL types per dialect, including nullability, default values, enums, and dates.
  - [x] Generate portable C++ representations for nullability, dates, times, datetimes, UUIDs, and JSON values discovered by `pull`.
  - [x] Introduce lossless decimal and binary value types with bindings for every enabled driver.
  - [x] Preserve column default expressions through introspection, manifests, generated entities, drift detection, inspection output, and DDL.
  - [x] Preserve native enum definitions through introspection, manifests, generated entities, and DDL.
- [x] Define how to preserve manual customizations when generated entities are regenerated.
- [x] Add tests with small schemas to validate both directions: database to entity and entity to database.
- [ ] Evaluate `sync` command as a way to compare C++ entities with the database schema and synchronize them.

### Database manipulation commands

- [ ] Evaluate commands `migrate`, `seed`, `n-plus-one`, `[migration] status` e `inspect`.
- [ ] Define the usage syntax for each one of the following commands (if accepted):
  - [ ] `migrate`: Apply pending migrations to the database, updating the schema from a previous version to the version expected by the application.
  - [ ] `seed`: Populate the database with pre-defined initial or test data—such as default users, permissions, settings, categories, or fixtures—for development and testing.
  - [x] `n-plus-one`: Detect N+1 query patterns—instances where an initial query triggers multiple unnecessary, repetitive queries to load related data.
  - [ ] `status`: Display the current state of the database relative to the project: applied and pending migrations, potential schema discrepancies, and other status information.
  - [x] `inspect`: Introspect the database and display its actual structure: schemas, tables, columns, data types, PKs, FKs, indexes, and other metadata.

## Milestone 5 — Developer experience

- [x] Create a quick start that works in less than ten minutes.
- [x] Document the minimum build, entity, CRUD, query, transaction, and error flow.
- [ ] Maintain complete examples for CRUD, transactions, queries, and relationships.
- [ ] Produce error messages that identify entity, field, column, and operation.
- [x] Document ownership, lifetime, and thread-safety guarantees.
- [ ] Create API documentation with Doxygen or an equivalent tool.
- [ ] Publish an architecture guide and relevant technical decisions.
- [ ] Add a changelog and migration guide for breaking changes.

## Milestone 6 — Quality, security, and performance

- [x] Run formatting and static analysis automatically in CI.
- [ ] Enable strict warnings and treat project warnings as errors in CI.
- [ ] Add sanitizers on Linux and equivalent tooling on Windows.
- [ ] Measure coverage and publish relevant gaps without chasing only a percentage.
- [ ] Add property tests and fuzzing for parsing, SQL generation, and SQL parameters.
- [ ] Create benchmarks for hydration, snapshots, and query generation.
- [ ] Measure statement preparation cost per driver and decide whether there should be a reusable per-connection cache.
- [ ] Measure connection opening cost and decide whether there should be a connection pool.
- [x] Define a security policy and responsible disclosure channel.
- [ ] Audit logs and exceptions to never expose passwords or sensitive parameters.

## Milestone 7 — Portability and distribution

- [x] Configure CI for Windows and Linux with MSVC, GCC, and Clang.
- [ ] Test minimum and current supported compiler versions.
- [ ] Create CMake install and export rules with `find_package(Worm)`.
- [ ] Publish reproducible packages on vcpkg and, if there is demand, Conan.
- [x] Allow minimal SQLite-only builds.
- [ ] Define and follow semantic versioning.
- [ ] Establish objective criteria for alpha, beta, and `1.0.0` releases.

## Milestone 8 — Community and sustainability

- [x] Adopt the MIT license.
- [x] Create `CONTRIBUTING.md` with setup, tests, and review criteria.
- [x] Adopt a code of conduct.
- [ ] Create templates for bugs, proposals, and pull requests.
- [ ] Maintain a public list of limitations and out-of-scope decisions.
- [ ] Recognize contributors and record important decisions openly.

## Criteria for version 1.0

- [ ] Public API documented with a compatibility policy.
- [ ] CRUD, transactions, parameters, and hydration validated on the supported databases.
- [ ] No critical manual ownership or known SQL vulnerability.
- [ ] Installable package and an example consumed by a clean external project.
- [ ] Stable CI, sanitizers, static analysis, and integration tests.
- [ ] Published getting started guide, reference, limitations, and contribution process.
- [x] Update README.md with the main project information.
