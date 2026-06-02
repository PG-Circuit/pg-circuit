# Releasing PG Circuit Community

## Checklist

1. **Version bump**
   - `include/pg_circuit.h` (`PG_CIRCUIT_VERSION` / `PG_CIRCUIT_VERSION_NUM`)
   - `pg_circuit.control` `default_version`
   - `Makefile` `EXTVERSION` / `DATA`
   - CLI `cmd/pgcircuit/main.go` `version`
   - `CHANGELOG.md`
   - Bump `PG_CIRCUIT_SHMEM_VERSION` if shared-memory layout changes

2. **SQL**
   - For Community 0.1.x, keep a single `sql/pg_circuit--0.1.0.sql` (or add upgrade scripts when introducing 0.2.0+)

3. **Tests**
   - `make installcheck` on PG 16/17/18
   - `go test ./...`
   - Optional: sanitizer job green

4. **Docs**
   - README / docs for user-visible changes
   - Sync [pgcircuit.com](https://pgcircuit.com) when cutting a release

5. **Tag**
   - `git tag -a vX.Y.Z -m "pg_circuit X.Y.Z"`
   - `git push origin vX.Y.Z`

6. **Artifacts** (GitHub Release on tag)
   - source tarball + sha256
   - `pgcircuit` CLI binaries (linux/darwin amd64/arm64)
   - checksums file
