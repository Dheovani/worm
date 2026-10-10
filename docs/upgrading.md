# Upgrading Worm

Worm is currently pre-release and does not provide API or binary compatibility guarantees. Upgrade deliberately: read `CHANGELOG.md`, rebuild from a clean CMake directory when build contracts change, and validate generated SQL and schema plans before applying them to persistent data.

## Current upgrade status

No stable Worm release has been published and there is no earlier supported release line requiring a version-specific migration recipe. Until the first release, breaking changes are documented under `Unreleased` in the changelog and in the pull request that introduces them.

## Upgrade checklist

Generated-key entity insertion now fails before SQL execution: `Repository::insert(entity)` and non-empty `insert(vector)` calls throw `MappingException` when the primary key is marked `generated`. Previously, an insert could write a row before reporting the missing returned key, or appear to work with a custom client that returned rows. Use application-provided keys for this API, or an explicit parameterized `insert(Statement)` with application-managed lookup. The statement overload returns affected rows, not a hydrated entity. Existing generated-key rows can still be read and updated. Do not retry earlier failed inserts against persistent data without checking whether they already wrote a row.

1. Read the `Added`, `Changed`, `Deprecated`, `Removed`, `Fixed`, and `Security` entries between the old and new revisions.
2. Search the codebase for renamed or removed public symbols, CMake options, environment variables, and CLI options identified by those entries.
3. Configure a clean build directory so stale CMake cache values and vcpkg features cannot preserve removed behavior.
4. Run unit tests and the enabled driver contracts before updating a deployed application.
5. Run `worm check`, `worm diff`, and migration validation against a disposable or backed-up database before applying schema changes.

## Types of breaking change

### C++ source API

A source break includes renamed headers or symbols, changed concepts, signatures, ownership, return types, exceptions, or lifecycle requirements. Update includes and call sites, then recompile every consumer. Worm does not promise binary compatibility, so consumers must relink even when their source still compiles.

### Build and driver configuration

A build break includes renamed CMake targets or options, changed required compiler versions, vcpkg feature changes, and newly optional or required dependencies. Check the [compiler support matrix](compiler-support.md), then delete or replace the affected build directory when a cached toolchain or manifest feature no longer matches the selected drivers.

### Runtime configuration

A configuration break includes renamed environment variables, changed defaults, new required values, or altered validation. Compare the current `.env.example` and configuration documentation without copying credentials into version control.

### Database schema and migrations

Updating the Worm library and migrating an application database are separate operations. Never infer that compiling against a newer revision makes an existing schema compatible. Review generated artifacts, validate their checksums, inspect destructive or ambiguous operations, back up persistent data, and apply migrations through the documented migration workflow.

Migration artifacts are immutable history. Do not edit an artifact that was already applied to any environment; create a later migration that moves the schema from the recorded state to the new state. Worm rejects checksum divergence rather than silently accepting rewritten history.

## Recording future breaks

A pull request that introduces a breaking change must update the `Unreleased` changelog and this guide when consumers need more than a direct rename. The entry should identify the old contract, the replacement, required data or configuration steps, and whether rollback is possible. The [release policy](releases.md) determines the required semantic-version increment and promotes those entries into a dated version section when a release is prepared.
