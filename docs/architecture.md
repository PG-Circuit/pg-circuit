# Architecture notes (Community)

## Event history

WARN/BLOCK decisions are stored in a **shared-memory ring buffer**:

- sized by `pg_circuit.event_history_size` (POSTMASTER, default 256)
- no network, no files, no unbounded growth
- exposed as `SELECT * FROM pg_circuit_events()`
- requires `shared_preload_libraries = 'pg_circuit'`

## Runtime signals

Transaction, lock, replication, WAL, and connection signals are collected for
`pg_circuit_runtime_state()` and the CLI. Community always reports safety mode
**NORMAL** (no AUTO→PROTECT/EMERGENCY transitions or incident correlation).

## WAL pressure

Uses public `pgstat_fetch_stat_wal()` and a cached byte-rate sample in shared
memory (refreshed at most once per second on the risky-query path). Displayed
in runtime status; Community does not add WAL pressure as a risk-rule finding.

## Observability

No outbound telemetry from the C extension. Prefer:

```text
Postgres extension ←local SQL→ Go CLI/exporter → Prometheus/OTel
```
