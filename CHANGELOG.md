# Changelog

This file records notable user-visible changes to Worm. The project is still pre-release and does not yet guarantee API or binary stability, so all current work remains under `Unreleased` until the first release policy is adopted.

The format follows the categories from Keep a Changelog without claiming semantic-versioning guarantees that Worm has not adopted yet.

## Unreleased

### Added

- Browsable Doxygen API documentation with local CMake generation, pull-request artifacts, and GitHub Pages publishing from `main`.
- An architecture guide and a record of the technical decisions governing reflection, statements, drivers, persistence contexts, migrations, and optional dependencies.
- An upgrade guide for handling source, build, configuration, and database compatibility changes during the pre-release period.
- Opt-in sanitizer and coverage instrumentation, with Linux and Windows sanitizer CI plus published Linux coverage reports.
- Deterministic property tests, a bounded Clang/libFuzzer CI target, and opt-in core and driver performance benchmarks.
- A canonical public limitations list and a contributor recognition policy linked from the project documentation.

### Changed

- Test and code-quality workflows skip expensive pull-request jobs and all documentation-only `main`-branch runs while preserving required pull-request statuses.

### Fixed

- Logs and normalized database exceptions now redact credential patterns, configured passwords, and sensitive parameter values echoed by drivers.

### Removed

- No user-visible removals have been recorded in this changelog yet.

## Maintaining this file

Add an entry under `Unreleased` in the same pull request as a user-visible addition, change, fix, deprecation, security correction, or removal. Describe observable behavior rather than implementation details. When a release process is established, move those entries into a dated version section and add comparison links without rewriting earlier release notes.
