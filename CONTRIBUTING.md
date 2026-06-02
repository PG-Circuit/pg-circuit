# Contributing to PG Circuit Community

Thank you for contributing.

Please follow the [Code of Conduct](CODE_OF_CONDUCT.md).

## Principles

1. Performance and stability beat feature count.
2. Decisions must be deterministic and explainable.
3. Never replace parse-tree inspection with regex shortcuts.
4. Never embed the Go runtime in PostgreSQL backends.
5. Keep v0.x scope focused — prefer a complete vertical slice over partial breadth.

## Development setup

```bash
# PostgreSQL 16/17/18 with server headers
make
sudo make install
make installcheck

# or
make docker-test
```

## Code style

- Match existing C style in `src/`
- Chain all hooks; never overwrite without preserving previous hooks
- Use PostgreSQL memory contexts correctly; avoid leaks on hot paths
- Treat compiler warnings as errors (`-Werror`)

## Tests

Add or update regression tests under `test/sql/` and `test/expected/` for behavioral changes.

## Pull requests

- Explain the risk/behavior change clearly
- Include test updates
- Keep commits focused
