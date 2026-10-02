# Technical decisions

This document records the architectural decisions that currently constrain Worm. They describe implemented behavior and should be updated when a deliberate replacement changes one of these boundaries.

## TD-001: Use static reflection for persistent fields

**Status:** Accepted.

**Context:** C++20 does not provide the general runtime reflection expected by traditional ORMs. A global runtime registry would add initialization order, synchronization, ownership, and type-erasure costs.

**Decision:** Entities expose a compile-time tuple of typed field descriptors through `reflect()`. Concepts validate the descriptor owners and the persistence metadata required by tables and views.

**Consequences:** Metadata errors can fail at compile time, typed hydration remains possible, and no global registration phase exists. Entity declarations are more explicit, and changing a persistent member requires changing its reflection descriptor as part of the same source update.

## TD-002: Keep SQL text and values separate

**Status:** Accepted.

**Context:** Interpolating application values into SQL creates injection risks, quoting errors, type ambiguity, and inconsistent behavior across databases.

**Decision:** `Statement` contains SQL text and an ordered vector of typed parameters. Builders choose dialect placeholders, and drivers bind values through native prepared-statement APIs.

**Consequences:** Generated SQL remains inspectable without exposing parameter values. Cache keys must include both SQL and parameters, and DDL remains a distinct reviewed path because identifiers and schema structure cannot be ordinary bound values.

## TD-003: Keep dialect rendering separate from driver execution

**Status:** Accepted.

**Context:** PostgreSQL, MySQL, SQLite, and SQL Server differ in placeholders, types, generated values, DDL, metadata catalogs, transactions, locks, and diagnostics. A single generic implementation would hide differences it cannot represent correctly.

**Decision:** `QueryBuilder` owns database-independent orchestration, `SqlBuilder` implementations render dialect-specific SQL, and `Client` implementations own native connection and binding behavior.

**Consequences:** Generic persistence code does not include native database headers. Supporting a new database requires explicit SQL, driver, schema, diagnostics, and contract work rather than only changing a connection factory.

## TD-004: Expose persistence through repositories and sessions

**Status:** Accepted.

**Context:** Public arbitrary execution through `Client` would bypass operation validation, identity mapping, snapshots, cache invalidation, and normalized persistence errors.

**Decision:** `Client::execute` remains private. `Session` composes the configured client, query builder, registry, and typed repositories; repositories expose CRUD and controlled manual-statement overloads.

**Consequences:** Application persistence follows one coherent path and manual SQL remains validated for the requested operation. Schema inspection, transactions, and migration locks receive narrow privileged access because their responsibilities cannot be expressed as entity CRUD.

## TD-005: Scope identity and thread affinity to a persistence context

**Status:** Accepted.

**Context:** Strong identity semantics require shared instances and snapshots within a defined lifetime. Making a native connection and all managed objects transparently cross-thread safe would not make entity mutation, transaction ownership, or database session state safe.

**Decision:** One `Registry` stores an `InstanceRegistry` per entity type, and repositories return `shared_ptr` managed instances. `Client`, `Session`, registries, and transactions enforce affinity to their creating thread.

**Consequences:** Repeated lookup within one context can preserve object identity and partial updates can use snapshots. Parallel workflows need independent sessions and connections, and rollback currently requires discarding or rebuilding registry state when in-memory objects were changed.

## TD-006: Make migrations explicit, immutable, and reviewable

**Status:** Accepted.

**Context:** Schema differences do not always contain enough information to infer safe DDL, rollback SQL, or cross-database transaction behavior. Automatically guessing destructive transformations would risk data loss.

**Decision:** Schema comparison produces semantic differences and risk classifications. Dialect compilation rejects unsupported transformations. Immutable migration artifacts store exact forward SQL, optional authored rollback SQL, database identity, and a content checksum before execution.

**Consequences:** Users can inspect the exact operations that will run, history can detect changed artifacts, and unsupported changes fail before partial plan generation. Some schema changes require manual migration authoring, especially SQLite table rebuilds and operations whose constraint names are unavailable.

## TD-007: Keep database drivers optional

**Status:** Accepted.

**Context:** Requiring every native database library would increase build time, package size, platform requirements, and failure surface for applications using only one database.

**Decision:** Each driver is controlled by an independent CMake option and matching vcpkg feature. Disabled drivers do not add sources, headers, libraries, or driver-specific tests to the build.

**Consequences:** No-driver and single-driver configurations are supported contracts. Shared code must not rely on transitive native headers, and CI must continue validating representative optional-driver combinations.
