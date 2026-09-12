# PG Circuit packaging (Community)

Source of truth for what **Community** ships in this public repository.
Marketing pages on [pgcircuit.com](https://pgcircuit.com) must match this document.

## Hard rules

1. **Community stays free.** Basic ALLOW / WARN / BLOCK for dangerous SQL stays Apache 2.0. No phone-home license check for Community.
2. **Apache stays Apache.** Community Runtime + basic CLI remain Apache 2.0, including commercial self-use and forks.
3. **Pay for Pro depth, coordination, and people** — not for permission to run Community `pg_circuit`.
4. **Cloud is optional.** Single-cluster Community never requires SaaS. If a hosted control plane is down, on-box enforcement still runs.

## This repository

| Edition | What it is | License |
|---------|------------|---------|
| **Community** (this repo) | On-box breaker + basic CLI | Apache 2.0 — free forever |

## Community (free) — in scope

- SQL parsing and basic dangerous-query detection
- `UPDATE` / `DELETE` without `WHERE`
- Destructive DDL rules
- Local config + basic CLI
- Basic on-box logs
- Best-effort support via GitHub Issues

## Commercial lines (not in this repo)

Documented on [pgcircuit.com/pricing](https://pgcircuit.com/pricing):

| Edition | What it is |
|---------|------------|
| **Pro** | Extended on-box engine + official binaries (policies, blast-radius, local policy exceptions, migration checks, richer audit) |
| **Cloud** | Hosted control plane, fleet UI, agents |
| **Enterprise** | Support contract + engagement on the official line (available without Cloud) |

## Explicit decisions (locked)

| Decision | Answer |
|----------|--------|
| Community Apache forever? | **Yes** — basic dangerous-query floor |
| Cloud required for single cluster? | **No** |
| Enterprise without Cloud? | **Yes** |
| License / phone-home for Community? | **Never** |

## What we never sell

- A license bit to run Community ALLOW / WARN / BLOCK
- Mandatory Cloud for single-cluster protection
- Gating basic dangerous-query protection behind SaaS

## Related

- [TRADEMARK.md](TRADEMARK.md) — name and official builds
- [SUPPORT.md](SUPPORT.md) — community vs commercial contacts
- [LICENSE](LICENSE)
- Site: [Pricing](https://pgcircuit.com/pricing) · [Cloud](https://pgcircuit.com/cloud) · [Enterprise](https://pgcircuit.com/enterprise)
