# Worm CLI

`worm` is the command-line interface for inspecting database schemas, generating Worm entity declarations, and running opt-in query diagnostics.

The implemented commands are `check`, `pull`, the safe initial form of `push`, `inspect`, and `n-plus-one`. `check` compares a JSON entity manifest with the selected database. `pull` introspects database tables and plans or generates C++ entity headers. `push` plans and creates missing tables while leaving incompatible existing tables unchanged. `inspect` prints the database structure supported by the selected driver. `n-plus-one` analyzes observed read-only SQL without opening a database connection or executing a statement.

## Build

Enable the CLI when configuring Worm:

```bash
cmake -S . -B build -DWORM_BUILD_CLI=ON
cmake --build build --target WormCli
```

With a multi-configuration generator such as Visual Studio, specify the configuration:

```powershell
cmake --build build --config Debug --target WormCli
```

The resulting executable is named `worm` (`worm.exe` on Windows). Applications using Worm as a library do not need to build the CLI.

The CLI only supports drivers enabled in the same CMake configuration:

| Database | CMake option | CLI driver name |
| --- | --- | --- |
| PostgreSQL | `WORM_ENABLE_POSTGRESQL` | `postgresql` |
| MySQL | `WORM_ENABLE_MYSQL` | `mysql` |
| SQLite | `WORM_ENABLE_SQLITE` | `sqlite` |
| Microsoft SQL Server | `WORM_ENABLE_MSSQL` | `mssql` |

Driver libraries remain optional. A disabled driver is not compiled or linked into the CLI.

## Usage

The command syntax is:

```text
worm [global-options] <check|pull|push|inspect|n-plus-one> [command-options]
```

Show the built-in reference or version:

```bash
worm --help
worm --version
```

Minimal PostgreSQL example:

```bash
worm \
  --manifest build/worm-schema.json \
  --driver postgresql \
  --host localhost \
  --database application \
  --username postgres \
  --password-env WORM_DATABASE_PASSWORD \
  check
```

Minimal SQLite example:

```bash
worm \
  --manifest build/worm-schema.json \
  --driver sqlite \
  --database data/application.db \
  check
```

## Schema manifest

Static C++ reflection cannot be discovered by scanning an arbitrary executable at runtime. The CLI therefore receives the code-side schema through `--manifest <path>`.

The current manifest format is JSON version 1:

```json
{
  "version": 1,
  "entities": [
    {
      "name": "User",
      "schema": "public",
      "table": "users",
      "columns": [
        {
          "name": "id",
          "type": "int64",
          "nullable": false,
          "generated": true,
          "unique": false
        },
        {
          "name": "role_id",
          "type": "int64",
          "nullable": false,
          "generated": false,
          "unique": false
        },
        {
          "name": "email",
          "type": "string",
          "length": 255,
          "nullable": false,
          "generated": false,
          "unique": true
        }
      ],
      "primaryKey": ["id"],
      "indexes": [
        {
          "name": "idx_users_email",
          "columns": [{"name": "email", "order": "asc"}],
          "unique": true
        }
      ],
      "foreignKeys": [
        {
          "name": "fk_users_role",
          "columns": ["role_id"],
          "referencedTable": "roles",
          "referencedColumns": ["id"],
          "onUpdate": "cascade",
          "onDelete": "restrict"
        }
      ]
    }
  ]
}
```

Each entity requires:

- a non-empty, unique `name`;
- a non-empty table name;
- at least one column with a unique name inside the entity;
- at least one primary-key column;
- primary-key names that refer to declared columns.

The optional `type` property uses Worm's canonical names: `boolean`, `int16`, `int32`, `int64`, `float32`, `float64`, `decimal`, `string`, `binary`, `date`, `time`, `datetime`, `uuid`, `json`, or `unknown`. Type modifiers are represented by `length`, `precision`, `scale`, `unsigned`, and `withTimeZone`. When a type is present, `check` compares its canonical kind with the database type normalized by the selected driver. Omitting it preserves compatibility with manifests that only describe structural metadata.

Indexes accept string column names or objects containing `name` and an optional `order` of `asc` or `desc`. Foreign keys require equally sized `columns` and `referencedColumns` arrays, accept an optional `referencedSchema`, and support `no-action`, `restrict`, `cascade`, `set-null`, and `set-default` for `onUpdate` and `onDelete`. Referenced tables are created before their dependents; inline foreign-key cycles are rejected instead of being partially applied.

The `schema` property is optional. Its default depends on the driver:

| Driver | Default schema |
| --- | --- |
| PostgreSQL | `public` |
| MySQL | the selected database |
| SQLite | `main` |
| Microsoft SQL Server | `dbo` |

The current CLI consumes this manifest but does not generate it from a compiled application yet.

## Configuration file

Connection and manifest options can be stored in `worm.toml`. Without `--config`, the CLI looks for `worm.toml` in the current working directory and continues normally when it does not exist.

```toml
[generator]
manifest = "build/worm-schema.json"

[database]
driver = "postgresql"
host = "127.0.0.1"
port = 5432
database = "application"
username = "worm"
password_env = "WORM_DATABASE_PASSWORD"
```

Select another file explicitly with:

```bash
worm --config config/development.toml check
```

Explicit command-line options take precedence over values read from the configuration file. An explicitly selected configuration file must exist and be a regular file.

Supported configuration keys are:

| Section | Keys |
| --- | --- |
| `[generator]` | `manifest`, `output`, `namespace` |
| `[database]` | `driver`, `host`, `port`, `database`, `username`, `password_env` |

`output` and `namespace` apply only to `pull` and do not affect `check`.

## Passwords

The CLI does not accept a literal database password option. Use an environment variable and pass its name through `--password-env` or `password_env`:

```bash
export WORM_DATABASE_PASSWORD='local-password'
worm --password-env WORM_DATABASE_PASSWORD check
```

PowerShell:

```powershell
$env:WORM_DATABASE_PASSWORD = 'local-password'
worm --password-env WORM_DATABASE_PASSWORD check
```

The referenced environment variable must exist. The resolved secret is not included in reports or reconstructed command output.

## The `inspect` command

`inspect` connects to the configured database and prints every schema object represented by the current driver introspection contract. It is read-only and deliberately has no command-specific options; connection settings and `--format` remain global options.

```bash
worm --driver postgresql --database application --username worm inspect
worm --driver sqlite --database data/application.db --format json inspect
```

Text output groups tables by schema and displays columns, native data types, nullability, generated and unique flags, primary keys, foreign keys, and indexes when those values are supplied by the driver. JSON output emits a `schemas` array containing the same structure. Schemas and tables are sorted by name so repeated inspection produces stable output.

## The `check` command

`check` is read-only. It never creates, alters, or removes database objects and never writes generated source files.

```bash
worm check
```

The command reports:

- entities whose tables are missing from the database;
- database tables without corresponding entities;
- missing and unexpected columns;
- nullability differences;
- generated-column differences;
- single-column uniqueness differences;
- primary-key column differences.

Limit the comparison to one or more entities:

```bash
worm check --entity User --entity Product
```

Or select tables instead:

```bash
worm check --table users --table products
```

`--entity` and `--table` cannot be combined. Unknown selections are rejected rather than treated as an empty successful comparison.

### Text output

Text is the default format:

```text
Worm

Code and database schemas are compatible.

Check summary

Discovery:
  Entities discovered     2
  Tables discovered       2
  Entities selected       2
  Tables selected         2

Comparison:
  Matched objects         2
  Compatible objects      2
  Incompatible objects    0
  Missing in code         0
  Missing in database     0
```

### JSON output

Use JSON for CI, scripts, and editor integrations:

```bash
worm --format json check
```

```json
{
  "command": "worm --format json check",
  "status": "drift",
  "info": "Schema drift detected.",
  "metrics": {
    "entitiesDiscovered": 2,
    "tablesDiscovered": 2,
    "entitiesSelected": 2,
    "tablesSelected": 2,
    "matched": 1,
    "compatible": 0,
    "incompatible": 1,
    "missingInCode": 1,
    "missingInDatabase": 1,
    "differences": [
      "public.users.email: nullability differs"
    ]
  }
}
```

### Exit codes

| Code | Meaning |
| ---: | --- |
| `0` | The command completed and no schema or diagnostic issue was found |
| `1` | Invalid input, configuration failure, database failure, or another execution error |
| `2` | Schema drift or diagnostic issues were detected |
| `3` | The requested operation was blocked by a safety rule; reserved for commands that can plan changes |

Exit code `2` allows CI to distinguish detected schema or query issues from an execution failure.

## The `n-plus-one` command

`n-plus-one` is an offline diagnostic command. It groups observed `SELECT` statements by normalized query shape and reports a pattern when its number of distinct executions exceeds the allowed maximum. SQL literals and supported placeholders are converted to internal parameters, and concrete literal values are never written to text or JSON reports.

Analyze one observed query:

```bash
worm n-plus-one --query "SELECT * FROM posts WHERE user_id = 42"
```

Analyze a semicolon-separated file containing multiple observed queries:

```bash
worm n-plus-one --file query-log.sql
```

Exactly one of `--query` and `--file` is required. Only read-only `SELECT` statements are accepted. Semicolons inside quoted SQL string literals do not split a statement.

The default maximum is one execution per normalized query pattern. Use `--max-executions` to permit a larger number before the command reports an issue:

```bash
worm n-plus-one --file query-log.sql --max-executions 3
```

The value must be a positive integer. The command exits with `0` when no pattern exceeds the limit, `2` when potential N+1 patterns are found, and `1` for invalid input or an execution error. Use the global `--format json` option for machine-readable metrics and findings.

## The `pull` command

`pull` discovers database tables and generates C++ entity headers. It is a dry run by default: the command validates selections and type mappings and reports the plan without creating directories or files.

```bash
worm --driver sqlite --database data/application.db pull --output src/entities --namespace application::entities
```

Use `--apply` to write the planned files:

```bash
worm --driver sqlite --database data/application.db pull --output src/entities --namespace application::entities --apply
```

Use repeatable `--table` options to select tables. `--name` is accepted only with exactly one selected table and changes the generated C++ type name.

```bash
worm --driver postgresql --database application pull --table users --name User --output src/entities --apply
```

Generated filenames use kebab-case, existing files are never overwritten, and each completed file is first written to a temporary sibling and then renamed. The current persistence model requires exactly one primary-key column, so tables without a primary key or with a composite primary key are rejected.

Generated entities follow a preserve-by-default policy: Worm never merges into or overwrites an existing source file. Regeneration must target a new path or happen only after the developer explicitly moves or removes the previous generated file. This keeps manual customizations under the developer's control instead of attempting an unsafe source-code merge.

The safe initial C++ mapping supports booleans, signed integers, 16-bit and 32-bit unsigned integers, floating-point values, strings, and dates. Nullable columns use `std::optional`. Decimal, binary, time, datetime, UUID, JSON, unknown types, and unsigned 64-bit integers are discovered but rejected during generation until Worm has lossless public representations and hydration support for them.

## Current comparison limitations

The normalized runtime snapshot does not yet represent every schema property. `check` currently does not compare:

- column types when the manifest omits the optional canonical `type` property;
- default values;
- foreign keys and referential actions;
- indexes as independent schema objects;
- views;
- composite unique constraints.

Composite unique constraints are ignored rather than incorrectly treating each participating column as individually unique. Database-specific metadata is normalized before comparison so the command produces the same high-level result model across supported drivers.

The `--verbose` and `--no-color` options are accepted by the current parser, but `check` does not yet emit an additional verbose diagnostic stream or ANSI-colored output.

## Local database containers

Disposable PostgreSQL and MySQL services for CLI development are defined in `tests/cli/docker-compose.yml`. Their Compose project, containers, database, and schema objects use Worm-specific names so they remain distinguishable from application services.

Start both databases:

```bash
docker compose -f tests/cli/docker-compose.yml up -d --wait
```

| Service | Container | Host port | Database | Username | Password |
| --- | --- | ---: | --- | --- | --- |
| PostgreSQL 17 | `worm-cli-postgres` | `15432` | `worm_cli` | `worm` | `worm` |
| MySQL 8.4 | `worm-cli-mysql` | `13306` | `worm_cli` | `worm` | `worm` |

These credentials are only for disposable local testing. Initialization scripts create `users` and `schema_contract` tables that exercise primary keys, generated values, nullability, single-column uniqueness, and composite uniqueness.

Remove the containers and their data after testing:

```bash
docker compose -f tests/cli/docker-compose.yml down --volumes
```

## The `push` command

`push` compares the selected manifest entities with the database and produces a plan by default. It does not modify the database unless `--apply` is present.

```bash
worm --driver sqlite --database data/application.db --manifest worm-schema.json push
worm --driver sqlite --database data/application.db --manifest worm-schema.json push --apply
```

Use repeatable `--entity` options to restrict the operation:

```bash
worm --driver postgresql --database application --manifest worm-schema.json push --entity User --apply
```

To review or apply the DDL through another deployment system, use `--output` instead of `--apply`:

```bash
worm --driver postgresql --database application --manifest worm-schema.json push --output build/worm-schema.sql
```

The generated SQL file contains only additive statements for objects missing from the inspected database. It is written atomically, an existing output file is never overwritten, and no database change is executed. `--output` and `--apply` are mutually exclusive so the command cannot ambiguously write and execute the same plan.

The current safe implementation creates missing tables, primary keys, supported generated columns, foreign keys, and indexes represented by schema metadata. Existing compatible tables are left untouched. Existing incompatible tables produce schema drift and are never altered automatically. Column values are not involved in DDL generation, identifiers are quoted by the selected dialect, and unknown column types are rejected before execution.

The manifest currently supplies columns, primary keys, indexes, and foreign keys. Defaults and database-native enum definitions are not represented yet. `push` does not currently perform `ALTER TABLE`, destructive synchronization, schema creation, view creation, or rollback generation.
