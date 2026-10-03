# Worm 🐛 (C++ With ORM)

Worm is a C++20 ORM inspired by Doctrine. The project is still in an early
stage, but it already provides a typed persistence flow built on static
reflection, parameterized SQL, RAII transactions, identity mapping, and optional
drivers for SQLite, PostgreSQL, MySQL, and SQL Server.

## Current status

- Optional drivers for PostgreSQL, MySQL, SQLite, and SQL Server.
- Typed CRUD with hydration, identity map, and partial updates based on
  snapshots.
- Bound parameters and RAII transactions.
- Optional dependencies controlled by CMake options and vcpkg features.
- Build organized around namespaced CMake targets (`Worm::*`).
- Unit tests and a shared integration contract for database drivers.
- Typed C++20 reflection with `constexpr` descriptors, the `Reflectable`
  concept, and field visitation.
- Parameterized expressions with `WHERE` and `ORDER BY` composition.
- An optional CLI with schema commands and offline N+1 query-pattern diagnostics.

Worm should not be considered production-ready yet. See [TODO.md](TODO.md) for
the roadmap.

## Real usage

The [getting started guide](docs/getting-started.md) shows a complete SQLite flow: CMake integration, reflected entity, CRUD, parameterized queries, transactions, errors, and ownership. The canonical [current limitations](docs/limitations.md) list defines the boundaries that still apply. The [examples index](examples/README.md) provides executable programs for reflection, hydration, CRUD, queries, relationships, sessions, transactions, schema comparison, and migration planning. The [architecture guide](docs/architecture.md) maps the subsystems and execution flow, while [technical decisions](docs/technical-decisions.md) explain the constraints behind them. The [API documentation guide](docs/api-documentation.md) explains how to generate the browsable Doxygen reference locally.

User-visible changes are recorded in the [changelog](CHANGELOG.md). Because Worm is still pre-release, consult the [upgrade guide](docs/upgrading.md) before moving an application between revisions that may change public APIs, configuration, or schema behavior. The [release policy](docs/releases.md) defines semantic-versioning rules and objective alpha, beta, release-candidate, and `1.0.0` gates.

The optional [Worm CLI](cli/README.md) can analyze one observed `SELECT` query or a semicolon-separated query log for repeated parameterized shapes without connecting to or executing statements against a database. See the [logging guide](docs/logging.md) for severity levels, structured fields, sensitive-data rules, and verbose CLI diagnostics.

## Requirements

- CMake 3.20 or newer.
- A supported C++20 compiler from the [tested compiler matrix](docs/compiler-support.md).
- Git.
- vcpkg.
- On Windows, Visual Studio 2022 Build Tools with the C++ workload.

Set `VCPKG_ROOT` to your vcpkg installation:

```powershell
$env:VCPKG_ROOT = "C:\Users\your-user\vcpkg"
```

Copy [.env.example](.env.example) to `.env` and adjust the selected driver and
credentials. `DATABASE_TYPE` accepts `sqlite`, `postgresql`, `mysql`, or
`mssql`; for SQLite, `DBNAME` may also be `:memory:`.

## Configure and build

On Windows with MSVC, the versioned presets are the recommended path:

```powershell
cmake --preset windows-msvc
cmake --build --preset debug
```

To build Release:

```powershell
cmake --build --preset release
```

By default, the [vcpkg.json](vcpkg.json) manifest installs libmysql, libpqxx,
and SQLite. Each driver can be disabled during configuration:

```powershell
cmake -S . -B build/sqlite `
  -DWORM_ENABLE_POSTGRESQL=OFF `
  -DWORM_ENABLE_MYSQL=OFF `
  -DWORM_ENABLE_SQLITE=ON `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
```

`WORM_ENABLE_POSTGRESQL`, `WORM_ENABLE_MYSQL`, and `WORM_ENABLE_SQLITE` are
independent and enabled by default. The SQL Server driver is enabled explicitly
with `WORM_ENABLE_MSSQL=ON` and uses ODBC. Disabled drivers do not add their
sources, tests, or dependencies to the build.

To use SQL Server, install Microsoft ODBC Driver 18 and configure
`MSSQL_ODBC_DRIVER`. The default name is `ODBC Driver 18 for SQL Server`.

## Tests

```powershell
ctest --preset debug
```

To run a single domain:

```powershell
ctest --test-dir build -C Debug -L errors --output-on-failure
ctest --test-dir build -C Debug -L connection --output-on-failure
ctest --test-dir build -C Debug -L core --output-on-failure
```

Tests live outside production code:

```text
tests/
├── connection/
├── core/
├── errors/
├── reflection/
└── utils/
```

SQLite, MySQL, and PostgreSQL share the same integration contract. SQLite runs
locally; the other databases use disposable CI services and can also be run
locally with the variables described in `.env.example`.

## Structure

```text
worm/
├── cmake/          # dependency discovery and normalization
├── docs/           # usage guides and limitations
├── examples/       # optional buildable examples
├── src/
│   ├── connection/ # database clients, configuration, factories, transactions
│   ├── core/       # ORM core, query model, persistence, hydration
│   ├── errors/     # public error types
│   ├── reflection/ # descriptors and typed field visitation
│   └── utils/      # helpers, hashing, dependency injection
├── tests/          # tests organized by subsystem
├── CMakeLists.txt
└── vcpkg.json
```

## Contributing and security

See [CONTRIBUTING.md](CONTRIBUTING.md), [CONTRIBUTORS.md](CONTRIBUTORS.md), [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md), and [SECURITY.md](SECURITY.md).

## License

This project is distributed under the [MIT license](LICENSE).
