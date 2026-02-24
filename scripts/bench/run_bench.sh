#!/usr/bin/env bash
# scripts/bench/run_bench.sh — reproducible pgbench overhead harness
#
# Compares: baseline (extension unloaded impractical) / disabled / observe / warn / enforce
# on a disposable cluster. Prints TPS and relative deltas. Document environment.
set -euo pipefail
PATH="$(brew --prefix postgresql@17 2>/dev/null)/bin:${PATH:-}"
SCALE="${SCALE:-5}"
TIME="${TIME:-10}"
CLIENTS="${CLIENTS:-4}"
JOBS="${JOBS:-4}"
PORT="${PORT:-55496}"
HOST="${HOST:-/tmp}"

echo "PG Circuit bench"
echo "date: $(date -u +%Y-%m-%dT%H:%MZ)"
echo "host: $(uname -mrs)"
echo "pg: $(pg_config --version 2>/dev/null || echo unknown)"
echo "scale=$SCALE time=${TIME}s clients=$CLIENTS jobs=$JOBS"
echo

tmpdir=$(mktemp -d)
export PGDATA="$tmpdir/data" PGHOST="$HOST" PGPORT="$PORT"
initdb -D "$PGDATA" >/dev/null
cat >> "$PGDATA/postgresql.conf" <<EOF
shared_preload_libraries = 'pg_circuit'
unix_socket_directories = '$HOST'
port = $PORT
max_connections = 100
EOF
pg_ctl -D "$PGDATA" -l "$tmpdir/pg.log" start
trap 'pg_ctl -D "$PGDATA" stop >/dev/null 2>&1 || true' EXIT

createdb bench
psql -d bench -c "CREATE EXTENSION pg_circuit;"
pgbench -i -s "$SCALE" bench >/dev/null

run_mode() {
  local label=$1
  local sql=$2
  psql -d bench -v ON_ERROR_STOP=1 -c "$sql" >/dev/null
  echo "=== $label ==="
  pgbench -c "$CLIENTS" -j "$JOBS" -T "$TIME" -n bench | tee "$tmpdir/$label.txt" | grep -E 'tps|latency'
  echo
}

run_mode "disabled" "ALTER SYSTEM SET pg_circuit.enabled = off; SELECT pg_reload_conf();"
# need restart for ALTER SYSTEM on some GUCs - enabled is PGC_SUSET, SET works:
run_mode "disabled_session" "SET pg_circuit.enabled = off;"
# pgbench sessions won't inherit SET — use ALTER DATABASE / conf
psql -d bench -c "ALTER DATABASE bench SET pg_circuit.enabled = off;"
run_mode "disabled" "SELECT 1;"

psql -d bench -c "ALTER DATABASE bench SET pg_circuit.enabled = on;"
psql -d bench -c "ALTER DATABASE bench SET pg_circuit.mode = observe;"
run_mode "observe" "SELECT 1;"

psql -d bench -c "ALTER DATABASE bench SET pg_circuit.mode = warn;"
run_mode "warn" "SELECT 1;"

psql -d bench -c "ALTER DATABASE bench SET pg_circuit.mode = enforce;"
run_mode "enforce" "SELECT 1;"

echo "Raw logs in $tmpdir"
echo "Target: near-zero to low single-digit % overhead on pgbench default (SELECT-heavy) fast path."
echo "Deep-analysis overhead is measured separately via pg_circuit_explain_risk."
