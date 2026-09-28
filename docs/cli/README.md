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
| `runtime` | Pressure score, lag, WAL pressure (display-only in Community) |
| `blockers` | Current lock wait edges |
| `events` | Bounded WARN/BLOCK history |
| `metrics` | Prometheus text / optional `--listen` scrape endpoint |
| `notify` | Watch events; POST JSON webhook on new BLOCK (optional WARN) |
| `break-glass` | `print` ticketed SET LOCAL snippet; `check` observe-mode guard |
| `check-sql` | Static warn scan of migration SQL files (CI) |
| `doctor` | Health checks + operator-loop tip |
| `version` | CLI version |

**When blocked:** `doctor` → `status` → `runtime` → `events`, then SQL `pg_circuit_explain_risk(...)`.

Pro-only CLI surfaces (policy, preflight, snapshot, incident, …) are not part of Community.

### Metrics

```bash
pgcircuit metrics                 # one-shot Prometheus text to stdout
pgcircuit metrics --listen :9187  # HTTP scrape endpoint GET /metrics
```

Exports runtime gauges (`pgcircuit_pressure_score`, lag, locks, WAL rate) plus
counts of WARN/BLOCK rows currently in the shared-memory event ring.
The extension itself never opens outbound connections — only this CLI process does.

### Notify (webhooks)

```bash
pgcircuit notify --url https://hooks.example/pg-circuit
PGCIRCUIT_WEBHOOK_URL=… pgcircuit notify --decisions BLOCK,WARN
pgcircuit notify --url … --interval 5s --dry-run --catch-up
pgcircuit notify --url http://127.0.0.1:9000/hook --allow-private
```

| Flag / env | Meaning |
|------------|---------|
| `--url` / `PGCIRCUIT_WEBHOOK_URL` | Destination (required unless `--dry-run`) |
| `--decisions` | `BLOCK` (default) or `BLOCK,WARN` |
| `--interval` | Poll period (default `5s`, min `1s`) |
| `--catch-up` | Also deliver events already in the ring at start |
| `--dry-run` | Print JSON payloads to stdout; no HTTP |
| `--allow-private` | Permit loopback / RFC1918 destinations (local testing) |

By default private/metadata hosts are rejected (SSRF hardening). Slack incoming
webhooks work: the payload includes a `text` field plus structured fields.

### Break-glass

```bash
pgcircuit break-glass print --reason INC-123 --ttl 15m
pgcircuit break-glass check
```

Community has no policy exception table — `print` emits a ticketed `SET LOCAL`
snippet for a short transaction. Pro adds TTL exceptions (`break-glass grant`).

### check-sql (CI)

```bash
pgcircuit check-sql migrations/*.sql
pgcircuit check-sql --fail-on-findings db/migrate/*.sql
```

GitHub Action: `.github/actions/warn-migrations` (warn-first by default).

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
- warns if `runtime_mode` is set to a non-`normal` value (ignored in Community)

It does not change configuration or data. On success it prints the status → runtime → events loop and an `explain_risk` example.

## Security model

- Extension stays local to PostgreSQL (no outbound telemetry).
- CLI is a SQL client only; same privilege model as any `psql` session.
- `notify` is the only Community command that makes outbound HTTP — opt-in via `--url`.

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
