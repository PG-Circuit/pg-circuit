# PG Circuit CLI (`pgcircuit`)

Operational client for **PG Circuit Community**. Runs **outside** PostgreSQL —
Go is never loaded into the database process.

## Installation

```bash
go build -o bin/pgcircuit ./cmd/pgcircuit
# optional
cp bin/pgcircuit /usr/local/bin/
```

## Connection

First match wins:

1. `--dsn`
2. `PGCIRCUIT_DSN`
3. `DATABASE_URL`

Credentials are never printed. `doctor` shows a redacted DSN only.

```bash
export DATABASE_URL='postgres://user:pass@localhost:5432/postgres'
pgcircuit status
```

## Commands

| Command | Purpose |
|---------|---------|
| `status` | Extension version, mode, thresholds |
| `runtime` | Pressure score, lag, WAL pressure |
| `blockers` | Current lock wait edges |
| `events` | Bounded WARN/BLOCK history |
| `doctor` | Non-destructive health checks |
| `version` | CLI version |

Pro-only CLI surfaces (policy, preflight, snapshot, incident, …) are not part of Community.

### Output formats

```bash
pgcircuit status --format text
pgcircuit status --format json
```

### Doctor

`pgcircuit doctor` verifies:

- PostgreSQL connectivity
- supported server major version (16–18)
- `pg_circuit` extension installed
- SQL inspection APIs callable

It does not change configuration or data.

## Security model

- Extension stays local to PostgreSQL (no outbound telemetry).
- CLI is a SQL client only; same privilege model as any `psql` session.

## Example

```text
$ pgcircuit status --dsn "$DATABASE_URL"
PG Circuit

PostgreSQL:       17.11
Extension:        0.1.0
Mode:             warn
Runtime mode:     NORMAL
...
```
