# Production hardening

## Failure philosophy

| Mode | Internal PG Circuit error |
|------|---------------------------|
| `observe` | **Fail open** — WARNING, allow statement |
| `warn` | **Fail open** — WARNING, allow statement |
| `enforce` | **Fail open** by default; **fail closed** if `pg_circuit.fail_closed = on` |

Intentional BLOCK decisions still raise `ERROR` with `ERRCODE_INSUFFICIENT_PRIVILEGE`.

## Memory contexts

| Allocation | Lifetime |
|------------|----------|
| Risk findings `List` / `StringInfo` | Current (query) memory context |
| Event ring + WAL sample | Shared memory (`PG_CIRCUIT_SHMEM_MAGIC` / version) |

Query-dependent strings must not be stored in shared memory without copying into fixed-size buffers (events use fixed `char[]` fields).

## Shared memory

- Magic + version checked on attach; mismatch disables event history until restart.
- Single LWLock tranche `pg_circuit` for ring + WAL cache.
- Event writes are bounded (`capacity`, wraparound via `head % capacity`).

## Hook coexistence

Hooks always chain to the previous hook (or `standard_*`):

- `planner_hook` — chain only (DML enforcement is in `ExecutorStart`)
- `ProcessUtility_hook` — check then chain
- `ExecutorStart_hook` — DML check then chain
- `ExecutorEnd_hook` — chain only

Load order: list other extensions before/after `pg_circuit` in `shared_preload_libraries` intentionally. Verify with `pg_stat_statements,pg_circuit` (or reverse) in staging.

## Prepared statements

DML risk checks run at **ExecutorStart**, so `PREPARE`/`EXECUTE` and the extended query protocol re-evaluate against current runtime mode and policies.

## Fault injection (developer builds)

```bash
make clean
make PG_CPPFLAGS="-I$(srcdir)/include -DPGCIRCUIT_FAULT_INJECTION"
```

Then:

```sql
SET pg_circuit.fault = 'runtime';      -- or event_full | replication
SET pg_circuit.mode = enforce;
SET pg_circuit.fail_closed = on;       -- optional
DELETE FROM t;                         -- exercises fail-open/closed
SET pg_circuit.fault = '';
```

Never enable in production builds.

## Compiler flags

`PG_CFLAGS` includes `-Wall -Wextra -Wformat -Wformat-security -Werror`.
`-Wshadow` / `-Wcast-qual` are omitted where they fight PostgreSQL headers.

## Sanitizers

CI job `sanitize` builds with ASan+UBSan when the toolchain allows. Sanitizers do not fully validate PostgreSQL core — they catch extension defects.
