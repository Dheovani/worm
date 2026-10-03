# Logging

Worm emits operational diagnostics through `worm::Logger`. Logging is disabled below `Warning` by default so normal library and CLI output remains quiet. The CLI option `--verbose` enables `Trace`, `Debug`, and `Info` diagnostics for that invocation. Logs are written to the diagnostic stream and never become part of command result output.

## Severity policy

- `Trace`: fine-grained execution flow, individual validation rules, metadata queries, and per-object processing. Trace events may be numerous and are intended for short diagnostic sessions.
- `Debug`: resolved non-sensitive options, internal state, generated plans, discovered metadata, timings, and result summaries useful during development.
- `Info`: high-level operation boundaries and successful steps that are relevant to an operator, such as starting or completing a command or migration.
- `Warning`: recoverable problems, skipped work, unsupported metadata, fallbacks, suspicious results, and destructive plans that have not failed the current operation.
- `Error`: failures that abort the current operation, including database, filesystem, validation, and unexpected runtime failures. An exception is logged once at the boundary that handles it.

`Off` disables every level. A configured minimum level includes that level and every more severe level.

## Structured fields

Use `Logger::log` when a diagnostic has searchable context. Keep the event message stable and place variable data in named fields:

```cpp
logger.log(
  LogLevel::Info,
  "Command execution finished.",
  {
    {"command", "inspect"},
    {"exit_code", "0"},
  });
```

The text sink renders fields as escaped `name="value"` pairs. Field names should be stable `snake_case` identifiers. Do not construct field names dynamically.

## Sensitive information

Never log passwords, tokens, complete connection strings, SQL parameter values, environment-variable contents, or entity data. SQL text may be logged only when external values remain represented by placeholders. Prefer counts, types, identifiers, elapsed time, and sanitized database metadata. Host, port, database, driver, file path, entity, table, and column names may be logged when they are necessary for diagnosis.

The text logger defensively replaces values of structured fields whose names identify passwords, secrets, tokens, credentials, authorization data, cookies, DSNs, connection strings, or private keys. It also redacts common key/value credential forms and URI user information from messages. Driver connection failures remove the configured password, and query failures remove echoed quoted strings, decimal values, and strings of at least four characters that match bound parameters. This is a safety boundary, not permission to pass sensitive values to logging APIs; callers must still follow the prohibition above.

## Exception boundaries

Log an exception only where it is handled or converted into a final user-facing result. Lower layers should add context through Worm exceptions and rethrow without logging. A user-facing error message is presentation; it does not justify logging the same exception from multiple internal layers.

## Library usage

A dedicated logger can write to any `std::ostream` and has an independent minimum level:

```cpp
std::ostringstream diagnostics;
Logger logger{diagnostics, LogLevel::Debug};
logger.debug("Schema plan contains %zu operations.", operationCount);
```

The process-wide `worm::logger` writes to `std::clog`. Change its threshold with `setMinimumLevel`; the logger serializes writes so individual records are not interleaved. The output stream must outlive any `Logger` that references it.

## Operational coverage

The CLI lifecycle, configuration resolution, database connections, parameterized SQL execution, schema introspection and comparison, migration planning, code generation, seed planning and execution, N+1 analysis, and filesystem operations emit diagnostics at their responsibility boundaries. Errors are logged at the boundary that converts them into a final operation result; internal layers add context to exceptions without logging and rethrowing the same failure.

SQL diagnostics contain placeholder-based SQL, binding counts, elapsed time, affected rows, and returned rows. Parameter values are never included. Executions taking at least one second produce a `Warning` diagnostic.
