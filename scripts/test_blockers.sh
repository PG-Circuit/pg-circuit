#!/usr/bin/env bash
# Bounded two-session lock-wait check against docker compose postgres.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

PSQL=(docker compose exec -T postgres psql -U postgres -d postgres -v ON_ERROR_STOP=1)

"${PSQL[@]}" -c "CREATE EXTENSION IF NOT EXISTS pg_circuit;"
"${PSQL[@]}" -c "DROP TABLE IF EXISTS blocker_demo; CREATE TABLE blocker_demo (id int primary key); INSERT INTO blocker_demo VALUES (1);"

# Holder keeps ACCESS EXCLUSIVE for several seconds
docker compose exec -T postgres \
  psql -U postgres -d postgres -v ON_ERROR_STOP=1 \
  -c "BEGIN; LOCK TABLE blocker_demo IN ACCESS EXCLUSIVE MODE; SELECT pg_sleep(4); ROLLBACK;" &
holder_pid=$!

sleep 0.4

# Waiter blocks (no short timeout) in background
docker compose exec -T postgres \
  psql -U postgres -d postgres \
  -c "BEGIN; LOCK TABLE blocker_demo IN ACCESS SHARE MODE; ROLLBACK;" &
waiter_pid=$!

sleep 0.6

# Inspect while the wait is active
"${PSQL[@]}" -c "SELECT blocked_sessions, blocking_sessions FROM pg_circuit_lock_summary();"
"${PSQL[@]}" -c "SELECT count(*) AS blocker_edges FROM pg_circuit_blockers();"

# Unblock by finishing holder; waiter should complete shortly after
wait "$holder_pid" || true
wait "$waiter_pid" || true

"${PSQL[@]}" -c "DROP TABLE IF EXISTS blocker_demo;"
echo "blocker check finished"
