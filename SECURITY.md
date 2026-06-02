# Security Policy

## Security model

PG Circuit is a native PostgreSQL extension loaded into backend processes. It:

- Inspects parse/plan trees and compact runtime signals to score risk
- May emit warnings or raise errors to block high-risk statements
- Does **not** send telemetry or open outbound network connections
- Does **not** execute shell commands or subprocesses
- Does **not** intentionally print passwords or secrets
- Does **not** mutate unrelated PostgreSQL settings
- Does **not** execute analyzed SQL as a side effect of assessment

Configuration is exposed via `pg_circuit.*` GUCs (typically `PGC_SUSET`). Loading via `shared_preload_libraries` is recommended so hooks are active for every session.

## Supported versions

| Version | Supported |
|---------|-----------|
| 0.9.x | Yes |
| 0.8.x | Security fixes while practical |
| &lt; 0.8 | Please upgrade |

## Reporting vulnerabilities

**Preferred:** email **[security@pgcircuit.com](mailto:security@pgcircuit.com)**

Alternatively, open a private [GitHub Security Advisory](https://github.com/PG-Circuit/pg-circuit/security/advisories/new) on this repository.

Please include:

- PostgreSQL major version
- Extension version (`SELECT pg_circuit_version();`)
- Minimal reproduction
- Impact assessment if known

Do **not** open public issues for undisclosed vulnerabilities.

We aim to acknowledge reports within a few business days.
