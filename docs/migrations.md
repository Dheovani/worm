# Migrations

Worm currently supports the safe planning and artifact representation parts of migrations: it can represent schema metadata, compare reflected entity metadata with an existing schema snapshot, create a reviewable migration plan, represent an immutable migration artifact, and keep migration history records with application state, failure state, and rollback timestamps. It does not execute generated migrations automatically, and the current migration plan intentionally keeps SQL statements optional because several differences require dialect-specific decisions that Worm should not guess.

## Current migration flow

The implemented flow is deliberately conservative:

1. Application or driver code provides a `SchemaMetadata` snapshot.
2. Worm compares a `PersistableEntity` with that snapshot through `compareEntityWithSchema<T>()`.
3. Worm maps differences to a `MigrationPlan` through `generateMigrationPlan(...)`.
4. Every generated step is reviewable. Ambiguous or destructive steps are marked as requiring manual review.
5. A reviewed plan can be represented as a versioned `MigrationArtifact` containing the exact forward SQL and optional, explicitly authored rollback SQL.
6. Migration artifacts can be serialized to canonical JSON, saved without overwriting an existing file, loaded, and checked against a SHA-256 content checksum.
7. `MigrationHistory` can record a migration id, checksum, state, application time, rollback time, and failure reason, but it is not yet persisted in the database.

This means Worm can tell that a table, column, primary key, or selected column metadata is missing or incompatible, but it does not yet decide the complete SQL type, default expression, constraint naming strategy, or destructive action policy for every database.

## Migration artifacts

`MigrationArtifact` is an immutable in-memory value with format version 1, a 14-digit sortable identifier, a name, a target database, a SHA-256 checksum, at least one forward statement, and optional rollback statements. Each statement records its description, exact SQL, and risk classification. Rollback is represented as `null` when unavailable; an empty rollback list is invalid so an irreversible migration cannot be confused with a migration whose rollback was accidentally omitted.

The CLI migration codec uses files such as `20260922143000_create-users.worm.json`. Loading is strict: unknown fields, unsupported versions or databases, malformed statements, empty required values, and checksum divergence are rejected. Saving uses the CLI's generated-file publishing path and never overwrites an existing artifact.

`MigrationCatalog` discovers `*.worm.json` artifacts directly inside a supplied directory, validates that each filename matches `<id>_<name>.worm.json`, and orders entries by ID independently of filesystem iteration order. Discovery is deliberately non-recursive, ignores unrelated files, rejects matching symlinks and non-regular paths, and rejects duplicate IDs. Comparing the catalog with historical `MigrationReference` values reports applied migrations whose local artifact is missing or whose current SHA-256 checksum differs from the recorded checksum; additional local artifacts are pending migrations and are not inconsistencies.

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

Worm treats migration generation as a review step, not an execution step. Missing tables and missing columns are currently marked as ambiguous because the system still needs a dialect-aware SQL type mapper before generating trustworthy DDL. Unexpected columns and primary key changes are marked as destructive because they may drop data or require rebuilding constraints. Nullability, generated-column, and uniqueness mismatches are marked as ambiguous because they can require data validation, constraint recreation, or full table definition depending on the database.

## Why SQL is optional in migration steps

`MigrationStep` can hold an optional `Statement`, but current generated steps intentionally do not include executable SQL. This is not a missing convenience; it is a safety boundary. A reviewable plan can be produced with the metadata available today, but executable DDL requires a later layer that knows the target dialect, type mapping, default values, naming strategy, and destructive-change policy.

## What remains before executable migrations

Before Worm can safely apply migration SQL, it still needs persistent database history, database-specific locking, complete dialect-aware DDL generation, table rebuild planning for SQLite, destructive-change confirmation, failure recovery policies, and integration tests against real database engines. Until those pieces exist, migration plans and artifacts should be treated as diagnostics and reviewable inputs only.
