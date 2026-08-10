# Production code conventions

These rules extend the root `AGENTS.md` for all content under `src/`.

## File names

- Use only lowercase letters in project-owned file names.
- Use `kebab-case` for compound names: `result-set.hpp`, `mysql-client.cpp`.
- Use `.hpp` for C++ headers and `.cpp` for implementations.
- Use `index.hpp` for a subsystem's public aggregator header.
- `CMakeLists.txt` keeps the spelling required by CMake and is an exception.

## C++ symbols

- Use `PascalCase` for classes, structs, enums, and concepts.
- Use `camelCase` for application functions, methods, and variables.
- Use `snake_case` only for metaprogramming symbols, such as traits, variable
  templates, and compile-time helper functions.
- Use `UPPER_SNAKE_CASE` only for macros and constants that intentionally follow
  that form.
- Declare subnamespaces with the compact syntax, such as
  `namespace worm::connection`; do not use nested namespace blocks like
  `namespace worm { namespace connection { ... } }`.
- Indent namespace contents by 2 spaces.
- Use a trailing `_` for private members, such as `connection_`.
- Format C++ with the repository `.clang-format`.
- Use 2-space indentation and no tab characters.
- Open braces on the next line for namespaces, classes, structs, enums,
  functions, and methods.
- Keep braces on the same line for control blocks such as `if`, `else`,
  `switch`, `for`, `while`, `try`, and `catch`.

## Modules

- Internal headers should include paths from `src`, such as
  `<core/query/expression.hpp>`.
- A module must not depend on examples or local tooling executables.
- Dependencies between modules must be expressed through `Worm::*` CMake
  targets.
- Concrete database implementations belong in `connection/drivers/`; the
  `connection/` root is reserved for contracts, configuration, factories, and
  transactions.
- ORM persistence components, such as repositories, registries, and future
  unit-of-work maps, belong in `core/persistence/`.
