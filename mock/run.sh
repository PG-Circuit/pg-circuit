#!/usr/bin/env bash
# mock/run.sh — seed mock data, run real queries, write results for git
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MOCK="$ROOT/mock"
OUT="$ROOT/test-results/mock-demo"
PATH="$(brew --prefix postgresql@17 2>/dev/null)/bin:${PATH:-}"
export PATH

mkdir -p "$OUT/queries" "$OUT/raw"

# disposable local cluster
tmpdir=$(mktemp -d)
export PGDATA="$tmpdir/data"
export PGHOST=/tmp
export PGPORT="${MOCK_PGPORT:-55511}"
export PGUSER
PGUSER="$(whoami)"
export PGDATABASE=mock_circuit

initdb -D "$PGDATA" >/dev/null
cat >> "$PGDATA/postgresql.conf" <<EOF
shared_preload_libraries = 'pg_circuit'
unix_socket_directories = '/tmp'
port = $PGPORT
EOF
pg_ctl -D "$PGDATA" -l "$tmpdir/pg.log" start
trap 'pg_ctl -D "$PGDATA" stop >/dev/null 2>&1 || true' EXIT

createdb "$PGDATABASE"

psql -v ON_ERROR_STOP=1 -f "$MOCK/seed.sql" | tee "$OUT/raw/00_seed.txt"

run_q() {
  local id="$1"
  local title="$2"
  local file="$3"
  local dest="$OUT/queries/${id}.md"
  {
    echo "# ${id} — ${title}"
    echo
    echo '```sql'
    cat "$file"
    echo '```'
    echo
    echo "## Result"
    echo
    echo '```text'
    psql -v ON_ERROR_STOP=0 -f "$file" 2>&1 || true
    echo '```'
  } > "$dest"
  echo "wrote $dest"
}

# numbered query files
i=0
for f in $(ls "$MOCK/queries"/*.sql | sort); do
  i=$((i + 1))
  base="$(basename "$f" .sql)"
  # base like 01_safe_select
  id="${base%%_*}"
  name="${base#*_}"
  title="${name//_/ }"
  run_q "$base" "$title" "$f"
done

python3 "$MOCK/assemble_report.py" "$OUT"

echo "DONE → $OUT"
