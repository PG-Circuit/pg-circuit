#!/usr/bin/env bash
# Microbenchmark Community risk scoring (no live collection)
set -euo pipefail
PATH="$(brew --prefix postgresql@17 2>/dev/null)/bin:${PATH:-}"
: "${DATABASE_URL:=postgres:///postgres}"

echo "Community explain_risk (delete_no_where):"
psql "$DATABASE_URL" -c "
SELECT avg(score) FROM generate_series(1, 100000) g,
  LATERAL pg_circuit_explain_risk('delete_no_where', 0, 0, 'normal', 0);"

echo "Community explain_risk (alter_table):"
psql "$DATABASE_URL" -c "
SELECT avg(score) FROM generate_series(1, 100000) g,
  LATERAL pg_circuit_explain_risk('alter_table', 0, 0, 'normal', 0);"
