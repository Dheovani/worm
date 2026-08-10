# Security Policy

## Supported versions

Worm is pre-1.0 software and does not yet publish stable release branches. Until
that changes, security fixes target the default development branch.

## Reporting a vulnerability

Please do not report security vulnerabilities through public issues.

Use GitHub private vulnerability reporting if it is enabled for the repository.
If it is not available, contact the maintainers privately and share only the
minimum information needed to establish a secure disclosure channel.

Useful reports include:

- A description of the vulnerability.
- Steps to reproduce it.
- The affected driver or subsystem.
- Whether credentials, query parameters, or database contents may be exposed.
- A suggested fix, if you already have one.

## Security goals

Worm's security posture is built around:

- Parameterized SQL instead of value interpolation.
- Explicit error flows between reflection, SQL generation, and drivers.
- Avoiding logs and exceptions that expose passwords or sensitive parameters.
- Optional drivers and minimal builds to reduce unnecessary dependency surface.
- Clear ownership and thread-safety rules.

## Current limitations

- The project has not completed a formal security audit.
- Fuzzing and sanitizer coverage are planned but not complete.
- SQL Server contract tests are not yet executed against a real CI instance.
- Generated migrations and schema synchronization are not implemented yet.

Please treat the project as experimental until the security and compatibility
policy matures.
