# Operations notes (Community)

## Upgrades / downgrades

Community **0.1.0** is a fresh SQL script (`sql/pg_circuit--0.1.0.sql`) with no
upgrade path from prior internal builds. Prefer `DROP EXTENSION` / reinstall
when iterating during development.

## Install constraints

Hooks and the in-memory event ring require `shared_preload_libraries = 'pg_circuit'`
**and a restart**. `CREATE EXTENSION` is still required after preload.

Managed Postgres services (RDS, Aurora, Cloud SQL, Azure Database, …) often
cannot load arbitrary shared libraries — prefer Docker or self-hosted Postgres
for Community installs.

## Privilege model

| Capability | Who |
|------------|-----|
| `pg_circuit_status/runtime/events/blockers/lock_summary/explain_risk*` | `PUBLIC` (EXECUTE) |
| GUCs (`pg_circuit.mode`, …) | Superuser / `PGC_SUSET` roles |

Roles `circuit_monitor` and `circuit_admin` are created at install (NOLOGIN)
for future privilege grants.

## Replication

| Role | Behavior |
|------|----------|
| Primary | Full enforcement |
| Physical hot standby | Reads only; utility/DML writes fail at PG before Circuit |
| Logical subscriber | Apply workers are not normal client backends; Circuit skips parallel workers |

## Backup / restore

`pg_dump` / `pg_restore` includes extension membership. Shared-memory event
history is **not** dumped (in-memory only).

## Security checklist

- [x] No `SECURITY DEFINER` functions in the extension
- [x] No outbound telemetry from the C extension
- [x] Fail-open by default (`fail_closed=off`); enforce may opt into fail-closed
