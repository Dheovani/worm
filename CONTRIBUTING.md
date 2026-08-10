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
- Public folder aggregators are named `index.hpp`.
- Project-owned file names use lowercase `kebab-case`.
- C++ uses C++20 and the root namespace `worm`.
- Classes, structs, enums, and concepts use `PascalCase`.
- Application methods and variables use `camelCase`.
- Metaprogramming helpers may use `snake_case`.
- Format C++ with the repository `.clang-format`.

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
