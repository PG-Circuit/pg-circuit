#!/usr/bin/env bash
# scripts/test_upgrade.sh — Community install smoke (no multi-version upgrade path)
set -euo pipefail
PATH="$(brew --prefix postgresql@17 2>/dev/null)/bin:${PATH:-}"
: "${PGPORT:=55495}"
: "${PGHOST:=/tmp}"
tmpdir=$(mktemp -d)
export PGDATA="$tmpdir/data" PGHOST PGPORT
initdb -D "$PGDATA" >/dev/null
cat >> "$PGDATA/postgresql.conf" <<EOF
shared_preload_libraries = 'pg_circuit'
unix_socket_directories = '$PGHOST'
port = $PGPORT
EOF
pg_ctl -D "$PGDATA" -l "$tmpdir/pg.log" start
trap 'pg_ctl -D "$PGDATA" stop >/dev/null 2>&1 || true' EXIT

psql -v ON_ERROR_STOP=1 -d postgres <<'SQL'
CREATE EXTENSION pg_circuit;
SELECT extversion FROM pg_extension WHERE extname = 'pg_circuit';
SELECT pg_circuit_version();
SELECT enabled, mode, effective_runtime_mode FROM pg_circuit_status();
SQL
echo "install ok"
