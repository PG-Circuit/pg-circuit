# PG Circuit Community

Apache 2.0 **Community** line for PG Circuit — a reduced on-box circuit breaker for PostgreSQL.

> Basic dangerous-query protection stays free forever. No phone-home license check.

## What this repo is

| In scope | Out of scope |
|----------|----------------|
| SQL parsing and basic dangerous-query detection | Extended Pro engine |
| `UPDATE` / `DELETE` without `WHERE` | Official Pro binaries / packages |
| Destructive DDL rules | Hosted Cloud control plane |
| Local config + basic CLI | Enterprise support contracts |
| Basic on-box logs | |

## Related product lines

| Edition | Repo | License |
|---------|------|---------|
| **Community** (this repo) | [`pg-circuit`](https://github.com/PG-Circuit/pg-circuit) | Apache 2.0 |
| **Pro** | [`pg-circuit-pro`](https://github.com/PG-Circuit/pg-circuit-pro) | Commercial |
| **Cloud** | `pg-circuit-cloud` | Proprietary |
| **Marketing** | [`pg-circuit-web`](https://github.com/PG-Circuit/pg-circuit-web) | — |

Product boundaries: [pg-circuit-pro/PACKAGING.md](https://github.com/PG-Circuit/pg-circuit-pro/blob/main/PACKAGING.md)  
Website: [pgcircuit.com](https://pgcircuit.com)

## Status

Community source is being split out here. Until the reduced tree lands, use the docs on [pgcircuit.com/docs](https://pgcircuit.com/docs) and the packaging matrix above.

## License

Apache License 2.0 — see [LICENSE](LICENSE) (will ship with the Community tree).
