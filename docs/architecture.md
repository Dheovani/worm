# Worm architecture

Worm is a C++20 ORM organized around a typed persistence boundary. Static entity metadata becomes parameterized SQL through database-aware builders; drivers execute the resulting statements and return database-independent result objects; a persistence context then hydrates and tracks entities. The architecture favors explicit contracts over hidden queries or global state.

## System map

```text
Application entity
      |
      v
Static reflection ------> Entity and schema metadata
      |                              |
      v                              v
Repository <------ Session ------ QueryBuilder
      |                              |
      |                              v
      |                     dialect SqlBuilder
      |                              |
      +-------- Statement <----------+
                    |
                    v
             connection::Client
                    |
                    v
             database driver
                    |
                    v
           ResultSet and hydration
                    |
                    v
          Registry and snapshots
```

## Subsystems

### Reflection

`src/reflection/` provides compile-time field descriptors, concepts, lookup, visitation, and snapshots. A reflected type exposes `reflect()` as a tuple of typed field descriptors. The `Reflectable` concept verifies that every descriptor belongs to the reflected type, which allows persistence metadata and hydration to retain concrete C++ member types without dynamic field registration.

### Model

`src/core/model/` defines tables, views, columns, constraints, relationships, schema snapshots, migration artifacts, and the entity concepts that combine those contracts. Persistable entities expose compile-time `table()`, `primaryKey()`, and reflection metadata. Views are queryable but cannot enter mutation paths intended for entities.

Relationship objects describe one-to-one, one-to-many, and many-to-many joins. They currently produce explicit query relations; they do not automatically load related objects or execute cascade policies.

### Query construction

`src/core/query/` separates semantic query data from SQL rendering. `Predicate`, `Filter`, `Criteria`, ordering, grouping, pagination, and sources are value objects. `QueryBuilder` orchestrates operations and produces a `Statement`; the selected `SqlBuilder` owns placeholder syntax, identifier rules, supported DDL, and other database-specific SQL differences.

`Statement` is the execution boundary and always keeps SQL text separate from typed parameters. Application and entity values must remain in its parameter vector rather than being interpolated into SQL.

### Connection and drivers

`src/connection/` contains the common client contract, configuration, diagnostics, schema inspection, migration locking, and RAII transactions. `src/connection/drivers/` contains PostgreSQL, MySQL, SQLite, and SQL Server implementations. Driver headers, sources, dependencies, and tests are selected independently by CMake options.

Raw statement execution is not application-facing. `Client::execute` is private and the persistence, schema-inspection, transaction, and migration components receive only the access required by their contracts. Driver exceptions are translated into Worm errors before crossing the public boundary.

### Output and hydration

`src/core/output/` owns database-independent results, rows, pagination, and typed hydration. Drivers translate native values into `ResultSet`; hydration maps columns back to reflected fields, distinguishes SQL `NULL`, and rejects missing or incompatible values with model, field, column, and operation context.

### Persistence context

`src/core/persistence/` contains repositories, `Session`, identity maps, snapshots, the query cache, and migration persistence. `Session` owns one client, one shared `Registry`, one dialect-aware `QueryBuilder`, and lazily created repositories. Repositories return `shared_ptr` instances so repeated lookup of the same identity in one registry can return the same managed object.

Snapshots record reflected field state. Updates compare the supplied entity with its snapshot and send only changed persistent fields when the entity is already managed. Successful mutations keep instances and snapshots coherent; removals invalidate their identity-map entries.

## Execution flow

1. The application obtains a typed repository from a thread-bound `Session`.
2. The repository validates entity metadata and converts reflected values into typed parameters.
3. `QueryBuilder` delegates database syntax to the configured `SqlBuilder` and returns a `Statement`.
4. The repository executes the statement through its client access and receives a `ResultSet`.
5. Read operations hydrate rows and reuse registered identities; mutations update or invalidate snapshots and cached results.

Manual `Statement` overloads remain controlled escape hatches. Repositories validate that the supplied SQL operation matches the invoked method, and filtered `UPDATE` and `DELETE` operations retain their qualifier-aware safety checks.

## Ownership, lifecycle, and concurrency

`Client`, `Session`, `Registry`, and their repositories belong to the thread where they were created. They are not made cross-thread safe through internal mutexes; concurrent work uses independent sessions and connections. Repositories share ownership of their client and registry, while returned entity pointers may outlive the session without making the entity itself thread-safe.

Transactions use RAII for cleanup but require explicit `commit()` or `rollback()`. An active transaction leaving scope attempts rollback. Rolling back database state does not automatically restore every in-memory entity already modified in the registry, so the affected persistence context should be discarded or rebuilt.

## Extension points

- A database integration implements `Client`, `SqlBuilder`, dialect behavior, schema inspection, diagnostics, and driver contract tests while remaining behind its CMake option.
- A persistable model supplies static table, primary-key, reflection, and snapshot contracts; no global entity registry is required.
- A new schema operation first enters the semantic migration plan, then receives explicit dialect compilation, execution-policy handling, and tests for unsupported transformations.
- Public errors derive from `WormException`; new boundaries translate implementation-specific failures while preserving useful operation and model context.

## Architectural limits

Worm does not currently provide a connection pool, reusable prepared-statement cache, automatic relationship loading, automatic cascade execution, cross-thread persistence contexts, or reconciliation of registry state after transaction rollback. SQL Server does not yet have the same service-backed integration coverage as PostgreSQL, MySQL, and SQLite. Installed builds can be consumed through `find_package(Worm)`, while publication in a package registry remains a later release step.

The rationale behind the major boundaries is recorded in [technical decisions](technical-decisions.md). Public usage starts in the [getting started guide](getting-started.md), the canonical cross-project boundaries are maintained in [current limitations](limitations.md), and migration-specific behavior and database limitations are documented in [migrations](migrations.md).
