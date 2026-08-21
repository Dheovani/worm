# Worm CLI

`worm` is the official command-line interface for Worm.

It provides tooling for inspecting, generating, and validating the relationship between Worm C++ entities and relational database schemas.

The initial CLI focuses on bidirectional schema generation:

| C++ entity | Database table | Action                                              |
| ---------- | -------------- | --------------------------------------------------- |
| Exists     | Missing        | Generate the database table                         |
| Missing    | Exists         | Generate the C++ entity                             |
| Exists     | Exists         | Compare both definitions and report inconsistencies |

The CLI deliberately avoids silently deciding which side should overwrite the other when both already exist.

Its primary goal is to make schema management predictable, inspectable, and safe.

---

# Executable

The command-line executable is:

```text
worm
```

On Windows:

```text
worm.exe
```

The recommended CMake target structure is:

```text
WormCli
Worm::Cli
```

with the output executable named:

```text
worm
```

For example:

```cmake
add_executable(WormCli ...)
add_executable(Worm::Cli ALIAS WormCli)

set_target_properties(
    WormCli
    PROPERTIES
    OUTPUT_NAME worm
)
```

The CLI can be enabled through:

```text
WORM_BUILD_CLI
```

Example:

```bash
cmake -S . -B build -DWORM_BUILD_CLI=ON
cmake --build build --target WormCli
```

The CLI is a development-time tool and is not required by applications using Worm at runtime.

---

# Why `worm`?

The executable is intentionally named `worm` rather than `worm-generator`.

Schema generation is only one responsibility of the CLI.

The command may eventually expose additional development tooling such as:

```text
worm migrate
worm migration
worm schema
worm inspect
worm doctor
```

Or even `worm sync`. Therefore, generation should be considered a capability of the Worm CLI rather than the identity of the executable itself.

The general syntax is:

```text
worm [global-options] <command> [command-options]
```

---

# Architecture

Worm entities are described through static C++ metadata, while database schemas are discovered through database introspection.

The CLI normalizes both representations into a common schema model:

```text
C++ entities ─────────┐
                      ├── SchemaMetadata ── comparison ── generation plan
Database schema ──────┘
```

Conceptually:

```text
CodeSchema
    │
    ▼
SchemaMetadata
    ▲
    │
DatabaseSchema
```

The comparison layer determines whether each object is:

```text
Missing in database
Missing in code
Compatible
Incompatible
```

Commands then decide what to do with that information.

---

# Code Schema Discovery

Worm reflection is static.

Because of that, the CLI cannot reliably discover arbitrary Worm entities simply by reading a compiled executable.

Applications should therefore expose their entity metadata through a schema manifest generated as part of the build process.

Conceptually:

```text
Application entities
        │
        ▼
Worm reflection metadata
        │
        ▼
Schema manifest
        │
        ▼
worm
```

The exact serialization and generation mechanism of the manifest is an implementation detail and may evolve independently from the CLI interface.

The manifest is supplied through:

```text
--manifest <path>
```

Example:

```bash
worm --manifest build/worm-schema.json check
```

---

# Commands

The initial CLI consists of three main commands:

```text
worm check
worm push
worm pull
```

Their directions are intentional:

```text
push
C++ ───────────────► Database

pull
C++ ◄────────────── Database

check
C++ ◄──── compare ────► Database
```

There is intentionally no generic `sync` command.

A command named `sync` would need to decide which side takes precedence when incompatible definitions exist.

Worm avoids making that decision implicitly.

---

# `check`

Compares the code schema with the database schema without modifying either side.

```bash
worm check [options]
```

Example:

```bash
worm \
    --manifest build/worm-schema.json \
    --driver postgresql \
    --host localhost \
    --database application \
    --username postgres \
    check
```

The command detects:

* entities without corresponding tables;
* tables without corresponding entities;
* missing columns;
* extra columns;
* incompatible column types;
* nullability differences;
* primary key differences;
* generated-column differences;
* other schema metadata incompatibilities supported by Worm.

Example output:

```text
Worm

✓ User <-> users
✓ Product <-> products

! Order <-> orders
    column total
      code:     double NOT NULL
      database: DECIMAL(10,2) NOT NULL

+ Session
    missing database table: sessions

- audit_logs
    missing C++ entity

Schema drift detected.

2 compatible
1 incompatible
1 missing in database
1 missing in code
```

`check` never modifies files or the database.

This makes it suitable for CI:

```bash
worm check
```

A project can therefore fail its CI pipeline whenever entity definitions and the database schema diverge.

---

# `push`

Uses C++ entity metadata to generate missing database objects.

```bash
worm push [options]
```

Direction:

```text
C++ ───────────────► Database
```

Example:

```bash
worm push
```

By default, `push` performs a dry run.

Example:

```text
Worm

Database changes:

+ CREATE TABLE users
+ CREATE TABLE sessions

No changes were applied.

Run with --apply to apply this plan.
```

To execute the generated operations:

```bash
worm push --apply
```

## Existing tables

`push` only creates database objects that do not exist.

If both the entity and table already exist, Worm compares them.

For example:

```text
User <-> users

Mismatch:
    email

    code:
        VARCHAR(255) NOT NULL

    database:
        VARCHAR(100) NULL
```

The initial version of the CLI should **not automatically execute `ALTER TABLE` operations** in this situation.

Instead, it reports the inconsistency.

This prevents potentially destructive schema changes from being performed simply because an entity definition changed.

Automatic migrations should be handled by a dedicated migration system in the future.

---

## Push a specific entity

```bash
worm push --entity User
```

Apply it:

```bash
worm push --entity User --apply
```

Multiple entities may be selected:

```bash
worm push \
    --entity User \
    --entity Product \
    --entity Order
```

---

# `pull`

Uses database metadata to generate missing Worm entities.

```bash
worm pull [options]
```

Direction:

```text
C++ ◄────────────── Database
```

Example:

```bash
worm pull
```

By default, `pull` performs a dry run.

Example:

```text
Worm

Entities to generate:

+ users
    -> src/entities/user.hpp

+ products
    -> src/entities/product.hpp

No files were written.

Run with --apply to generate these entities.
```

To write the files:

```bash
worm pull --apply
```

---

## Pull a specific table

```bash
worm pull --table users
```

Apply it:

```bash
worm pull --table users --apply
```

Multiple tables may be selected:

```bash
worm pull \
    --table users \
    --table products \
    --table orders
```

---

## Output directory

The output directory can be selected explicitly:

```bash
worm pull \
    --output src/domain/entities \
    --apply
```

---

## Namespace

The generated C++ namespace can be specified:

```bash
worm pull \
    --namespace application::model \
    --apply
```

For example:

```cpp
namespace application::model
{
    // Generated entity
}
```

---

## Explicit entity name

Database table names are not always suitable C++ class names.

For a single table, an explicit entity name can therefore be supplied:

```bash
worm pull \
    --table users \
    --name User \
    --apply
```

This is useful because Worm should avoid relying on linguistic assumptions such as automatically converting every plural table name into a singular English noun.

For example:

```text
people
status
data
categories
news
```

cannot all be reliably transformed using a simple pluralization rule.

Without an explicit mapping, the generator should use a deterministic naming strategy rather than attempting to infer grammar.

---

# Existing Entities

`pull` must never silently overwrite an existing C++ entity.

Consider:

```text
Database:
    users

Code:
    User
```

If both definitions exist, Worm compares them.

If they differ:

```text
! User <-> users

Schema mismatch detected.
Existing entity will not be modified.
```

Even when `--apply` is specified, existing entity files are not rewritten by the initial implementation.

This is intentional.

Entity files may contain application-specific behavior unrelated to persistence, and blindly regenerating them could destroy user code.

Updating existing entities automatically should only be introduced if Worm eventually defines a safe mechanism such as generated regions or structural source-code transformations.

---

# Global Options

Global options configure the CLI itself and may be placed before the command:

```text
worm [global-options] <command> [command-options]
```

Available global options:

```text
-c, --config <path>       Configuration file
    --manifest <path>     Worm schema manifest
    --driver <driver>     Database driver
    --host <host>         Database host
    --port <port>         Database port
    --database <name>     Database name or SQLite database path
    --username <name>     Database username
    --password-env <var>  Read the database password from an environment variable
    --format <format>     Output format
    --verbose             Enable verbose output
    --no-color            Disable ANSI colors
-h, --help                Show help
-v, --version             Show version
```

Supported database drivers are expected to include:

```text
postgresql
mysql
sqlite
mssql
```

Supported output formats initially include:

```text
text
json
```

---

# Command Options

Options that only apply to a particular operation belong to the command rather than the global CLI.

## `check`

```text
--entity <name>       Check a specific entity
--table <name>        Check a specific table
```

Both options may be repeated.

---

## `push`

```text
--entity <name>       Push a specific entity
--apply               Apply the generated database plan
```

`--entity` may be repeated.

---

## `pull`

```text
--table <name>        Pull a specific table
--output <path>       Output directory for generated entities
--namespace <name>    C++ namespace for generated entities
--name <name>         Explicit entity name for a single table
--apply               Write generated files
```

`--table` may be repeated.

---

# Passwords

Database passwords should preferably not be passed directly as command-line arguments because command-line values may be stored in shell history or exposed through process inspection.

Instead:

```bash
export APP_DB_PASSWORD="secret"
```

and:

```bash
worm \
    --password-env APP_DB_PASSWORD \
    check
```

Configuration files should follow the same principle and reference environment variables instead of storing production credentials directly.

---

# Configuration File

Repeated CLI arguments may be stored in a project configuration file.

The recommended default filename is:

```text
worm.toml
```

Example:

```toml
[generator]
manifest = "build/worm-schema.json"
output = "src/entities"
namespace = "application::entities"

[database]
driver = "postgresql"
host = "localhost"
port = 5432
database = "application"
username = "postgres"
password_env = "APP_DB_PASSWORD"
```

With this configuration:

```bash
worm check
```

is equivalent to supplying the corresponding options manually.

A different configuration file can be selected with:

```bash
worm --config config/development.toml check
```

Command-line arguments override configuration-file values.

---

# Option Scope

The CLI distinguishes between global options and command-specific options.

For example:

```bash
worm \
    --config worm.toml \
    --verbose \
    push \
    --entity User \
    --apply
```

Here:

```text
--config
--verbose
```

are global options.

While:

```text
--entity
--apply
```

belong specifically to `push`.

This separation keeps command semantics clear and prevents unrelated options from becoming part of the global interface.

---

# Dry Runs

Any command capable of modifying the project or database must be non-destructive by default.

Therefore:

```bash
worm push
```

and:

```bash
worm pull
```

only produce plans.

Actual changes require:

```text
--apply
```

Examples:

```bash
worm push --apply
```

```bash
worm pull --apply
```

`check` never accepts `--apply` because it is intrinsically read-only.

---

# Machine-readable Output

Commands support machine-readable output through the global `--format` option.

Example:

```bash
worm --format json check
```

Possible output:

```json
{
  "status": "drift",
  "compatible": 12,
  "missingInDatabase": 2,
  "missingInCode": 1,
  "incompatible": 3
}
```

JSON output is useful for:

* CI pipelines;
* IDE integrations;
* build tooling;
* future GUI integrations;
* automated analysis.

Human-readable text remains the default.

---

# Exit Codes

The CLI should expose predictable exit codes.

| Code | Meaning                                                                                 |
| ---: | --------------------------------------------------------------------------------------- |
|  `0` | Operation completed successfully and no unresolved schema drift exists                  |
|  `1` | Execution error, invalid configuration, database failure, or internal error             |
|  `2` | Schema drift or incompatibility was detected                                            |
|  `3` | Requested operation was intentionally blocked because it would violate CLI safety rules |

For example:

```bash
worm check
```

returns `0` when code and database are compatible.

It returns `2` when differences are found.

This makes the command suitable for direct CI integration.

---

# Help

General help:

```bash
worm --help
```

Command-specific help:

```bash
worm push --help
```

```bash
worm pull --help
```

```bash
worm check --help
```

Version information:

```bash
worm --version
```

---

# Typical Workflow

## Code-first

Create an entity:

```text
src/entities/user.hpp
```

Generate or update the Worm schema manifest during the build:

```text
User
    table: users
```

Inspect the database changes:

```bash
worm push
```

Output:

```text
+ CREATE TABLE users
```

Apply:

```bash
worm push --apply
```

Then verify:

```bash
worm check
```

---

## Database-first

Suppose the database contains:

```text
users
products
orders
```

but the application has no corresponding Worm entities.

Inspect what would be generated:

```bash
worm pull
```

Generate the files:

```bash
worm pull --apply
```

Then verify:

```bash
worm check
```

---

# Bidirectional Does Not Mean Automatic Synchronization

Worm's schema tooling is bidirectional because it understands both directions:

```text
Code → Database

Database → Code
```

It does **not** mean that arbitrary differences are automatically reconciled.

For example:

```text
Code:
    User::email -> std::string

Database:
    users.email -> INTEGER
```

Neither side can safely be assumed to be correct.

The CLI therefore reports the conflict:

```text
User.email

code:
    string

database:
    integer

incompatible
```

The developer decides how the inconsistency should be resolved.

This distinction is fundamental to Worm's CLI design.

---

# Generator Responsibility

The entity generator is a subsystem of the Worm CLI.

Conceptually:

```text
Worm CLI
│
├── schema discovery
├── schema comparison
├── entity generation
├── database generation
└── future development tools
```

The executable itself should therefore not be modeled as a standalone `worm-generator` product.

Instead:

```text
worm
```

is the public interface, while generation logic remains an internal capability.

A possible project structure is:

```text
worm/
├── core/
├── connection/
├── ...
└── cli/
    ├── CMakeLists.txt
    └── src/
        └── main.cpp
```

As the CLI grows, it may evolve into:

```text
cli/
├── src/
│   ├── main.cpp
│   ├── commands/
│   │   ├── check.cpp
│   │   ├── push.cpp
│   │   └── pull.cpp
│   ├── generator/
│   ├── schema/
│   └── output/
└── CMakeLists.txt
```

The exact internal structure is not part of the public CLI contract.

---

# Future Extensions

The initial CLI intentionally focuses on schema generation and validation.

The architecture should allow future commands such as:

```text
worm migration create
worm migration apply
worm migration rollback

worm schema inspect
worm schema diff

worm doctor
```

A migration workflow could eventually support:

```text
Code
  │
  ▼
Schema diff
  │
  ▼
Migration plan
  │
  ▼
Migration file
  │
  ▼
Explicit execution
```

Potential future functionality includes:

* `ALTER TABLE` generation;
* migration files;
* migration history;
* migration rollback;
* destructive-change detection;
* rename detection;
* generated regions inside entity files;
* schema snapshots;
* custom type mappings;
* naming policies;
* include/exclude patterns;
* multiple database schemas;
* IDE integration.

These features should remain separate from the basic bidirectional generation contract.

---

# Design Principles

The Worm CLI follows a small set of rules.

## Explicit direction

The synchronization direction is always visible:

```text
push = code → database
pull = database → code
```

## Read-only comparison

`check` never changes anything.

## Dry-run by default

Commands that can modify the database or filesystem only produce a plan unless `--apply` is explicitly provided.

## Never overwrite silently

Existing entity files are never silently regenerated.

## Never guess precedence

When both sides exist and disagree, neither side is automatically considered authoritative.

## Deterministic generation

The same metadata and configuration should always produce the same generated schema or source code.

## CI-friendly behavior

Differences are represented through stable output and exit codes.

## Database-independent comparison

Database-specific metadata is normalized before comparison so the higher-level CLI logic remains independent from PostgreSQL, MySQL, SQLite, MSSQL, or future Worm database drivers.

---

# Summary

The core interface is intentionally small:

```bash
# Compare code and database
worm check

# Code -> database
worm push
worm push --apply

# Database -> code
worm pull
worm pull --apply
```

The fundamental behavior is:

```text
Entity exists + table missing
    -> push can create the table

Table exists + entity missing
    -> pull can create the entity

Entity exists + table exists
    -> compare them

Entity and table disagree
    -> report the conflict; do not silently choose a winner
```

The `worm` executable represents the complete Worm command-line interface.

Entity and schema generation are capabilities of that CLI, leaving room for migrations, schema inspection, diagnostics, and other development tooling to be added later without introducing additional top-level executables.
