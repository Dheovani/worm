# Migrations

Worm currently supports safe planning, dialect-specific DDL compilation, artifact representation, local validation, persistent history, locking, execution planning, and explicit application through `Repository<MigrationHistory>`: it can compare reflected metadata with a schema snapshot, create a reviewable plan, compile supported differences through the selected SQL builder, validate immutable migration artifacts, and execute a previously registered plan under the database-specific lock and transaction policy. The CLI does not yet expose migration application.

## Current migration flow

The implemented flow is deliberately conservative:

1. Application or driver code provides a `SchemaMetadata` snapshot.
2. Worm compares a `PersistableEntity` with that snapshot through `compareEntityWithSchema<T>()`.
3. Worm maps differences to a `MigrationPlan` through `generateMigrationPlan(...)`.
4. Every generated step is reviewable. Ambiguous or destructive steps are marked as requiring manual review.
5. A reviewed plan can be represented as a versioned `MigrationArtifact` containing the exact forward SQL and optional, explicitly authored rollback SQL.
6. Migration artifacts can be serialized to canonical JSON, saved without overwriting an existing file, loaded, and checked against a SHA-256 content checksum.
7. `MigrationHistory` represents migration records in memory, while `Repository<MigrationHistory>` creates or validates the Worm-owned `_worm_migrations` table and persists the artifact ID, name, SHA-256 checksum, state, application time, rollback time, and failure reason.
8. `worm migrate create --name <slug> --directory migrations` compares the configured manifest with the current database and writes the supported dialect-specific forward DDL as an immutable artifact without applying it or inferring rollback SQL.
9. `worm migrate validate --directory migrations` validates the local catalog ordering, filenames, artifact structure, and embedded checksums without connecting to the database or executing SQL. The directory defaults to `migrations` and can also be configured as `directory` under `[migrations]` in `worm.toml`.
10. `worm migrate status --directory migrations` compares that catalog with persistent history and classifies matching applied, pending, failed, missing, and checksum-divergent migrations without modifying the database.
11. `worm migrate rollback --directory migrations` validates persistent history and the local checksum, then reverts only the greatest applied migration ID using its explicitly authored rollback statements.
12. `compileMigrationExecutionPlan(...)` validates the artifact again, rejects a target database that differs from the selected `SqlBuilder`, preserves the reviewed statement order and risk classification, and selects the database's transaction boundary.
13. `compileMigrationDdl(...)` combines a `MigrationPlan`, expected `SchemaMetadata`, actual `SchemaSnapshot`, and selected `SqlBuilder` into ordered, parameter-free DDL statements. It rejects stale plans and transformations that require metadata Worm does not possess.
14. `MigrationExecutionPlan::policy()` derives the required confirmation and failure-recovery behavior from the reviewed statements and transaction mode. `authorizeMigrationExecution(...)` must accept that risk before the plan may run.
15. `Repository<MigrationHistory>::apply(...)` and `rollback(...)` acquire the migration lock, verify state and checksum under that lock, execute the statements under the selected transaction boundary, and persist the resulting state or failure.

## Migration locking

`Repository<MigrationHistory>::acquireLock(timeout)` acquires the lock associated with the configured migration-history schema and returns a move-only `MigrationLock`. The default timeout is 30 seconds. Calling `release()` completes the protected scope and releases the lock; if the object leaves scope while still active, its destructor performs the failure cleanup without throwing. A client cannot own two migration locks, start a regular transaction while a migration lock is active, or acquire a migration lock while a regular transaction is active. Lock operations preserve the client's thread-affinity rules.

PostgreSQL uses a session advisory lock derived from the migration lock name and retries non-blocking acquisition until the timeout. MySQL uses `GET_LOCK` and `RELEASE_LOCK`. SQL Server uses a session-owned exclusive `sp_getapplock`. SQLite has no advisory-lock facility, so it holds `BEGIN EXCLUSIVE` on the migration connection: explicit release commits the protected SQLite migration scope, while destruction of an unreleased lock rolls it back. MySQL may round a non-integral timeout up to the next whole second because `GET_LOCK` accepts seconds.

## Execution plans and transaction boundaries

`compileMigrationExecutionPlan(...)` is the boundary between an immutable, reviewed `MigrationArtifact` and database execution. It preserves the artifact checksum and compiles either the forward statements or the explicitly authored rollback statements into parameter-free `Statement` values. Compilation rejects checksum divergence, a database mismatch, and rollback requests for irreversible artifacts. It does not reinterpret or translate SQL from one database to another: the artifact's `database` field and the selected `SqlBuilder` must agree.

PostgreSQL and SQL Server use one transaction per migration. Their session-owned migration locks remain active around that transaction. MySQL uses no encompassing transaction because its DDL may commit implicitly, so failure recovery must account for partially applied migrations. SQLite's migration lock owns the exclusive transaction itself; starting another transaction inside that scope is rejected, successful release commits it, and scope-exit cleanup rolls it back. These modes are represented by `MigrationTransactionMode::PerMigration`, `None`, and `LockOwned` respectively so the eventual executor does not infer behavior from database names.

## Confirmation and failure recovery

The execution policy has three confirmation levels. A plan containing only safe statements requires `MigrationConfirmation::None`; any ambiguous statement raises the requirement to `Ambiguous`; any destructive statement raises it to `Destructive`. A destructive confirmation also covers ambiguous statements in the same plan. Core code does not display an interactive prompt: the CLI or embedding application must obtain confirmation and pass the accepted level to `authorizeMigrationExecution(...)`. Insufficient confirmation fails before any statement is executed.

Failure recovery follows the transaction boundary instead of assuming that all databases provide transactional DDL. `PerMigration` plans require rollback of the encompassing transaction. `LockOwned` plans require abandoning the lock-owned transaction so its scope cleanup rolls back the migration. Plans with no transaction boundary use `ManualReconciliation`, because one or more earlier statements may already have committed. `MigrationExecutionPolicy::mayBePartiallyApplied()` exposes that distinction to callers.

Worm never continues automatically from the statement following a failure. `Repository<MigrationHistory>::apply(...)` records the migration as failed after transaction or lock-scope cleanup and rejects later attempts because the record is no longer pending. Transaction-backed failures remove their schema changes before the failure record is written. With `ManualReconciliation`, earlier statements may remain committed, so the failed state requires explicit database inspection and reconciliation; replaying the full plan or guessing a resume position could repeat already committed DDL.

## DDL compilation

`compileMigrationDdl(...)` delegates rendering to the selected `SqlBuilder` and preserves the order, risk, description, and source difference for every migration step. A step may produce multiple statements: creating a PostgreSQL enum, its table, and its indexes is one example. The flattened statement list remains parameter-free because identifiers and reviewed DDL structure are not application values.

All builders compile table creation and removal, ordinary column addition and removal, and supported primary-key additions. PostgreSQL compiles type, nullability, and default alterations where the available metadata is unambiguous. MySQL restates the complete expected column definition for type, nullability, and default changes, and uses native `DROP PRIMARY KEY`/`ADD PRIMARY KEY` syntax. SQL Server restates type and nullability together as required by `ALTER COLUMN`. SQLite compiles operations supported directly by `ALTER TABLE` and explicitly requires a future table-rebuild plan for structural alterations it cannot express in place.

Compilation deliberately fails instead of guessing when the model lacks required information. Current examples include unique-constraint changes without constraint or index names, generated-column changes that do not distinguish identity from computed expressions, PostgreSQL and SQL Server primary-key replacement without the existing constraint name, SQL Server default changes without the existing default-constraint name, and SQLite changes requiring a table rebuild. These failures are `MigrationException`s with the affected step in the message; no partial DDL plan is returned.

This means Worm can tell that a table, column, primary key, or selected column metadata is missing or incompatible, but it does not yet decide the complete SQL type, default expression, constraint naming strategy, or destructive action policy for every database.

## Migration artifacts

`MigrationArtifact` is an immutable in-memory value with format version 1, a 14-digit sortable identifier, a name, a target database, a SHA-256 checksum, at least one forward statement, and optional rollback statements. Each statement records its description, exact SQL, and risk classification. Rollback is represented as `null` when unavailable; an empty rollback list is invalid so an irreversible migration cannot be confused with a migration whose rollback was accidentally omitted.

The CLI migration codec uses files such as `20260922143000_create-users.worm.json`. Loading is strict: unknown fields, unsupported versions or databases, malformed statements, empty required values, and checksum divergence are rejected. Saving uses the CLI's generated-file publishing path and never overwrites an existing artifact.

`MigrationCatalog` discovers `*.worm.json` artifacts directly inside a supplied directory, validates that each filename matches `<id>_<name>.worm.json`, and orders entries by ID independently of filesystem iteration order. Discovery is deliberately non-recursive, ignores unrelated files, rejects matching symlinks and non-regular paths, and rejects duplicate IDs. Comparing the catalog with historical `MigrationReference` values reports applied migrations whose local artifact is missing or whose current SHA-256 checksum differs from the recorded checksum; additional local artifacts are pending migrations and are not inconsistencies.

## Persistent history

`Repository<MigrationHistory>` keeps migration execution state in `_worm_migrations` in the configured database schema. Initialization creates the table when it is absent and refuses to use an existing table whose required columns or primary key are incompatible. State transitions use parameterized statements, require exactly one affected record, and store timestamps as signed epoch milliseconds so that the representation is consistent across supported drivers. The in-memory `MigrationHistory` remains the domain representation loaded from that durable table; it is not an alternative source of truth for executable migrations.

```json
{
  "checksum": "sha256:...",
  "database": "postgresql",
  "down": [
    {
      "description": "Drop users table",
      "risk": "destructive",
      "sql": "DROP TABLE users"
    }
  ],
  "id": "20260922143000",
  "name": "create-users",
  "up": [
    {
      "description": "Create users table",
      "risk": "safe",
      "sql": "CREATE TABLE users (id bigint PRIMARY KEY)"
    }
  ],
  "version": 1
}
```

## Cross-database limitations

Schema migration is not portable SQL with different placeholder syntax; each database has real semantic differences. Worm must keep those differences visible instead of hiding them behind a false common denominator.

| Area | PostgreSQL | MySQL | SQLite | SQL Server |
| --- | --- | --- | --- | --- |
| Transactional DDL | Most DDL can participate in transactions, but some operations have special restrictions. | Many DDL statements cause implicit commits depending on operation and engine. | DDL is transactional in common cases, but table rebuilds are often required for structural changes. | Many DDL statements are transactional, but locks and metadata behavior need explicit handling. |
| Alter column | Rich `ALTER TABLE ... ALTER COLUMN` support. | Uses `MODIFY`/`CHANGE` syntax and often requires restating the full column definition. | Limited native `ALTER TABLE`; many changes require creating a new table, copying data, and renaming. | Uses `ALTER TABLE ... ALTER COLUMN`, with restrictions around indexes, constraints, and nullability. |
| Generated/identity columns | Identity and generated expressions have distinct syntax and version-dependent behavior. | Auto-increment and generated columns differ from PostgreSQL identity semantics. | Rowid/integer primary key behavior is special; generated columns exist only in newer SQLite versions. | Identity columns and computed columns have SQL Server-specific syntax and restrictions. |
| Boolean values | Native boolean type maps naturally. | Boolean is commonly an alias for tiny integer semantics. | Boolean is stored using dynamic typing conventions. | Uses `bit` for boolean-like values. |
| Text and string sizes | `text`, `varchar(n)`, and collation rules are explicit. | Charset/collation and `varchar` limits matter. | Dynamic typing makes declared type less strict than other databases. | `nvarchar`, `varchar`, and collation choices matter. |
| Date/time | Several precise timestamp/date types with timezone considerations. | Multiple temporal types with precision and timezone caveats. | Usually stored as text, integer, or real by convention. | Several date/time types with different precision and timezone behavior. |
| Indexes | Rich index features such as partial/expression indexes. | Index length, prefix indexes, and engine behavior matter. | Partial/expression indexes exist, but support depends on SQLite version. | Filtered indexes and included columns are SQL Server-specific. |
| Foreign keys | Strong support and deferrable constraints are possible. | Foreign key behavior depends on engine and constraint details. | Foreign keys must be enabled and have limited alteration support. | Strong support, but alteration can require dropping dependent constraints first. |

## Current review policy

Worm treats migration generation as a review step, not an execution step. Missing tables and columns remain ambiguous because valid DDL can still fail against existing data or introduce unintended defaults. Unexpected columns and primary-key changes are destructive because they may drop data or rebuild constraints. Nullability, generated-column, uniqueness, and default mismatches remain ambiguous because they can require data validation or metadata that is not present in the current snapshot. Successful DDL compilation does not waive these risk classifications.

## Why SQL is optional in migration steps

`MigrationStep` can hold an optional `Statement`, but schema comparison keeps the diagnostic plan independent from a selected database. `MigrationDdlPlan` is the explicit dialect-bound result and can contain multiple statements per source step. Keeping these representations separate prevents a plan generated for one database from silently becoming executable under another dialect.

## What remains before executable migrations

The shared driver contract now covers history initialization, migration locking, generated dialect DDL, successful application, failure persistence, transactional rollback, MySQL partial application, checksum divergence, and rejection of automatic retries. PostgreSQL, MySQL, and SQLite execute that contract when their integration services are available; SQL Server still lacks equivalent service-backed integration coverage. The CLI exposes artifact creation, validation, status inspection, and rollback of the latest applied migration through explicitly authored rollback statements. Remaining work includes explicit SQLite table-rebuild artifacts and exposing migration application through the CLI.
