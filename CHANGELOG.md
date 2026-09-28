# Changelog

All notable changes to **PG Circuit Community** are documented in this file.

## Unreleased

### CLI (0.1.1)
- `pgcircuit metrics` — Prometheus text (one-shot or `--listen :9187` scrape endpoint)
- `pgcircuit notify` — poll `pg_circuit_events()` and POST JSON webhooks on new BLOCK (optional WARN)
- `pgcircuit break-glass print|check` — ticketed session override snippet + observe-mode guard
- `pgcircuit check-sql` — static warn scan for migration SQL (CI)
- GitHub Action: `.github/actions/warn-migrations`
- SSRF hardening on notify URLs (bypass with `--allow-private` for local hooks)
- `events` text/JSON output includes `event_id`

## 0.1.0

### Community release
- Native PostgreSQL extension with observe / warn / enforce modes
- Risk rules: PGC001–PGC005 (destructive DML/DDL), PGC012–PGC016 (ALTER / INDEX / REINDEX / VACUUM FULL / CLUSTER)
- Runtime pressure collection for display (transactions, locks, replication lag, WAL, connections)
- Effective safety mode always **NORMAL** (no auto PROTECT/EMERGENCY escalation)
- Shared-memory WARN/BLOCK event ring
- SQL APIs: `version`, `status`, `runtime_state`, `explain_risk`, `explain_risk_ex`, `blockers`, `lock_summary`, `events`
- Go CLI: `status`, `runtime`, `blockers`, `events`, `doctor`, `version`
- Policy / assess / incident / custom-rules / fleet APIs are **not** included (Pro)

### Release
- Public Community 0.1.0 cut on 2026-09-11.
