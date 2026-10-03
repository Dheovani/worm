# Contributing to Worm

Thanks for considering a contribution. Worm is still young, so the most useful
contributions are the ones that make behavior clearer, safer, and easier to
test.

## Development setup

You need:

- CMake 3.20 or newer.
- A C++20 compiler.
- Git.
- vcpkg.
- On Windows, Visual Studio 2022 Build Tools with the C++ workload.

Set `VCPKG_ROOT` before configuring the project:

```powershell
$env:VCPKG_ROOT = "C:\Users\your-user\vcpkg"
```

Then configure and build:

```powershell
cmake --preset windows-msvc
cmake --build --preset debug
ctest --preset debug
```

Top-level builds enable strict compiler warnings by default through `WORM_ENABLE_STRICT_WARNINGS`. Pass `-DWORM_WARNINGS_AS_ERRORS=ON` during configuration to reproduce the CI policy that rejects project warnings.

Pass `-DWORM_ENABLE_SANITIZERS=ON` to instrument project targets with AddressSanitizer on MSVC or AddressSanitizer and UndefinedBehaviorSanitizer on GCC and Clang. Use a non-Debug configuration with MSVC because its Debug runtime checks are incompatible with AddressSanitizer; CI uses `RelWithDebInfo` on Windows.

Pass `-DWORM_ENABLE_COVERAGE=ON` with GCC or Clang on a Unix-like platform to enable gcov-compatible instrumentation. CI runs the SQLite, CLI, and example suites, publishes detailed HTML and Cobertura XML artifacts, and places the text report in the workflow summary without enforcing an arbitrary percentage threshold.

Pass `-DWORM_BUILD_FUZZERS=ON` with Clang on a Unix-like platform to build the libFuzzer targets with AddressSanitizer and UndefinedBehaviorSanitizer instrumentation. Run `build/fuzz/fuzz/WormStatementFuzzer fuzz/corpus -max_total_time=30` to exercise SQL parsing, statement generation, and parameter preservation from the checked-in seed corpus; CI runs the same bounded fuzz session for every source change.

Pass `-DWORM_BUILD_BENCHMARKS=ON` to build `WormCoreBenchmarks`. Run the Release executable with an optional positive iteration count, for example `build/benchmarks/Release/WormCoreBenchmarks.exe 100000` on Windows or `build/benchmarks/WormCoreBenchmarks 100000` on a single-config platform; it writes CSV measurements for hydration, snapshot creation/change detection, and query generation without imposing timing thresholds on shared CI hardware.

The same option builds `WormDriverBenchmarks`, which accepts a driver name and optional iteration count. For example, `build/benchmarks/WormDriverBenchmarks sqlite 1000` measures connection opening/closing and a parameterized statement round-trip; external drivers read connection settings from `WORM_BENCH_<DRIVER>_HOST`, `_PORT`, `_DBNAME`, `_USERNAME`, and `_PASSWORD`, where `<DRIVER>` is `POSTGRES`, `MYSQL`, or `MSSQL`, while SQLite accepts `WORM_BENCH_SQLITE_DBNAME`. The statement result includes preparation, execution, decoding, and hydration because the public driver contract intentionally does not expose native preparation separately.

For a minimal SQLite-only build:

```powershell
cmake -S . -B build/sqlite `
  -DBUILD_TESTING=ON `
  -DWORM_ENABLE_POSTGRESQL=OFF `
  -DWORM_ENABLE_MYSQL=OFF `
  -DWORM_ENABLE_SQLITE=ON `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"

cmake --build build/sqlite --config Debug
ctest --test-dir build/sqlite -C Debug --output-on-failure
```

## Project conventions

- Production code lives in `src/`; tests live in `tests/`.
- Each subsystem has its own CMake target and `Worm::*` alias.
- Project-owned file names use lowercase `kebab-case`.
- C++ uses C++20 and the root namespace `worm`.
- Classes, structs, enums, and concepts use `PascalCase`.
- Application methods and variables use `camelCase`.
- Metaprogramming helpers may use `snake_case`.
- Format C++ with the repository `.clang-format`.
- Document new public APIs with Doxygen comments that describe observable contracts.
- Record user-visible and breaking changes under `Unreleased` in `CHANGELOG.md`.

See the root [AGENTS.md](AGENTS.md) and scoped `AGENTS.md` files for the full
repository rules.

## Tests

Add or update tests with every behavior change. Prefer focused unit tests for
small components and contract tests when behavior must be shared across drivers.

Useful commands:

```powershell
ctest --test-dir build -C Debug -L core --output-on-failure
ctest --test-dir build -C Debug -L connection --output-on-failure
ctest --test-dir build -C Debug --output-on-failure
```

Driver contract tests may require database-specific environment variables. See
[.env.example](.env.example).

To validate API documentation locally, follow [docs/api-documentation.md](docs/api-documentation.md). Documentation generation is optional for normal builds and is exposed through the `WormDocs` CMake target.

Pull requests always run a lightweight source-change check so required statuses can complete. When a pull request changes only Markdown, `docs/`, `.env.example`, the license, issue templates, or the documentation workflow, compilation, static analysis, driver services, and test matrices are skipped. Documentation-only pushes to `main` skip the test and code-quality workflows completely, while relevant documentation changes continue through the Doxygen workflow.

Architectural changes must update [docs/architecture.md](docs/architecture.md) and [docs/technical-decisions.md](docs/technical-decisions.md) when they replace or materially alter a recorded boundary. Breaking changes must also provide concrete consumer steps in [docs/upgrading.md](docs/upgrading.md).

Update [docs/limitations.md](docs/limitations.md) when a contribution adds, removes, or changes a documented boundary. After a person's first accepted contribution reaches the default branch, add their preferred public name and handle to [CONTRIBUTORS.md](CONTRIBUTORS.md); preserve co-author metadata when more than one person contributed to the change.

## Pull request expectations

A good pull request should:

- Explain the problem and the chosen solution.
- Keep unrelated formatting or refactoring out of the diff.
- Update documentation when public behavior changes.
- Add tests for new behavior or bug fixes.
- Preserve existing public contracts unless the change is intentional and
  documented.
- Avoid logging credentials, SQL parameters, or other sensitive values.

## Design priorities

When there is a tradeoff, prefer:

- Explicit behavior over hidden magic.
- Parameterized SQL over string interpolation.
- RAII and clear ownership over manual lifecycle management.
- Small, composable abstractions over broad framework-like APIs.
- Portable behavior first, dialect-specific behavior where necessary and
  documented.
