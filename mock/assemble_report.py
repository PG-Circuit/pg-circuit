#!/usr/bin/env python3
"""Build test-results/mock-demo/README.md from query markdown files."""
from __future__ import annotations

import datetime as dt
import json
import re
import sys
from pathlib import Path


def main() -> None:
    out = Path(sys.argv[1])
    queries = sorted((out / "queries").glob("*.md"))
    stamp = dt.datetime.now(dt.timezone.utc).strftime("%Y-%m-%dT%H:%MZ")

    blocks = 0
    warns = 0
    for q in queries:
        text = q.read_text(encoding="utf-8", errors="replace")
        blocks += len(re.findall(r"\bBLOCK\b|blocked high-risk|ERROR:\s+PG Circuit", text))
        warns += len(re.findall(r"WARNING:\s+PG Circuit", text))

    summary = {
        "stamp": stamp,
        "suite": "mock-demo",
        "queries": [q.stem for q in queries],
        "query_count": len(queries),
        "signal_hits": {"block_or_error_mentions": blocks, "warning_mentions": warns},
        "verdict": "CAPTURED",
    }
    (out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")

    lines = [
        f"# Mock demo results — `{stamp}`",
        "",
        "Real SQL against a local PostgreSQL with **mock shop data** + `pg_circuit` 0.1.0.",
        "",
        f"**Queries captured:** {len(queries)}  ",
        f"**BLOCK/ERROR hits in output:** {blocks}  ",
        f"**WARN hits in output:** {warns}",
        "",
        "## Dataset",
        "",
        "| Table | Rows (seed) |",
        "|-------|------------:|",
        "| `shop.customers` | 50 |",
        "| `shop.products` | 40 |",
        "| `shop.orders` | 200 |",
        "| `shop.order_items` | 500 |",
        "",
        "## Query index",
        "",
    ]
    for q in queries:
        title = q.read_text(encoding="utf-8", errors="replace").splitlines()[0].lstrip("# ").strip()
        lines.append(f"- [{title}](queries/{q.name})")

    lines += [
        "",
        "## How to regenerate",
        "",
        "```bash",
        "./mock/run.sh",
        "```",
        "",
        "Outputs land in `test-results/mock-demo/` (safe to commit).",
        "",
    ]

    # Append condensed excerpts
    lines += ["## Highlights", ""]
    for stem in (
        "03_enforce_delete_no_where",
        "04_warn_delete_allows",
        "06_enforce_truncate",
        "08_status_runtime",
        "12_events",
    ):
        path = out / "queries" / f"{stem}.md"
        if not path.exists():
            continue
        body = path.read_text(encoding="utf-8", errors="replace")
        # keep result fence only
        m = re.search(r"## Result\n\n```text\n(.*?)```", body, re.S)
        excerpt = (m.group(1).strip() if m else body)[-1200:]
        lines += [f"### `{stem}`", "", "```text", excerpt, "```", ""]

    (out / "README.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"report → {out / 'README.md'}")


if __name__ == "__main__":
    main()
