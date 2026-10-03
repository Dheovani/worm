# Current limitations

This document is the canonical public list of Worm's current limitations. It describes implemented behavior as it exists now, not planned behavior from the roadmap. More detailed subsystem constraints remain in the linked guides.

## Stability and distribution

- Worm is pre-release and does not currently guarantee source, API, ABI, configuration, or schema compatibility between revisions. Follow the [upgrade guide](upgrading.md) for every update.
- The supported minimum and current compiler versions have not been formally defined and tested yet, beyond the compiler matrix exercised by CI.
- CMake install and export rules are not available, so consumers cannot use `find_package(Worm)` from an installed package. Worm is not published through vcpkg or Conan.
- Semantic versioning and objective alpha, beta, and `1.0.0` release criteria have not been adopted yet.

## Database drivers

- PostgreSQL, MySQL, and SQLite run the shared driver contract in CI; PostgreSQL and MySQL use disposable services, while SQLite runs locally. SQL Server implements the ODBC driver contract but does not yet have equivalent service-backed CI coverage.
- Database-generated primary keys do not yet have one portable retrieval contract across all drivers.
- Database versions newer than the highest major version validated by Worm are reported by the CLI as warnings rather than treated as confirmed-compatible versions.
- Dialect differences remain visible. SQL, DDL, types, timeouts, permissions, locking, generated values, and transaction behavior cannot be assumed to behave identically across supported databases.

## Persistence and relationships

- Relationships describe explicit joins and query inclusion. Worm does not automatically load related objects, provide lazy loading, or execute cascade persistence and removal policies.
- `Client`, `Session`, `Repository`, `Registry`, and transactions are thread-affine. Parallel workflows require independent contexts and connections.
- A database rollback does not restore entity values already changed in memory or automatically reconcile registry snapshots. Discard or rebuild the affected persistence context after rollback when its managed objects were modified.
- The opt-in `SELECT` result cache cannot detect mutations made by other processes. Keep it disabled when external database changes must become visible immediately.

## Schema and migrations

- Migration generation covers the DDL documented in the [migration guide](migrations.md). Unsupported transformations require authored SQL, and SQLite table-rebuild plans are not generated automatically.
- Worm does not infer rollback SQL. Irreversible migration artifacts keep `down` as `null`, and rollback requires explicitly authored statements.
- Schema comparison does not yet normalize every database property. Indexes and foreign keys can be inspected but do not participate in CLI `diff`; consult the [CLI comparison limitations](../cli/README.md#current-comparison-limitations) for the complete property list.
- `push` does not currently generate schema creation, view creation, destructive synchronization, general `ALTER TABLE` plans, or rollback SQL. Worm deliberately provides no ambiguous `sync` command.
- Native enum DDL is supported only where the selected dialect can preserve its semantics. SQLite and SQL Server reject it instead of silently weakening the type to text.

## Performance and diagnostics

- Worm does not provide a connection pool or reusable prepared-statement cache. Applications should keep explicit thread-local sessions alive for an appropriate scope and measure their own workloads before adding external pooling.
- The N+1 detector is opt-in and analyzes observed parameterized query shapes. It does not collect statements globally, inspect parameter values, block execution, or automatically rewrite queries.
- In-memory `Paginator` operates on an already loaded `ResultSet`; it does not reduce the rows fetched from the database.
- API documentation is being adopted incrementally. The generated reference includes public symbols that do not yet have complete Doxygen descriptions.

## Maintaining this list

Update this document in the same pull request that adds, removes, or materially changes a limitation. Link to detailed documentation instead of duplicating long operational guidance, and remove an entry only after implementation and relevant tests demonstrate that the limitation no longer applies.
