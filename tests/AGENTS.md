# Test conventions

These rules extend the root `AGENTS.md` for all content under `tests/`.

- Mirror the subsystem organization from `src/` inside `tests/`.
- Create one dedicated test file for each tested component.
- Name files as `<component>-tests.cpp`, always in `kebab-case`.
- Register every executable in CTest and apply subsystem labels.
- Run the changed subsystem tests first and then the full suite.
- Do not add a test framework without an explicit project decision.
