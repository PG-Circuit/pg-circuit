# Changelog

All notable changes to **PG Circuit Community** are documented in this file.

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
