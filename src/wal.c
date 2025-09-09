/*-------------------------------------------------------------------------
 *
 * wal.c
 *		WAL pressure signal via pgstat_fetch_stat_wal()
 *
 * Checkpoint/IO pressure is postponed: cumulative checkpointer timings are
 * available via pgstat_fetch_stat_checkpointer(), but deriving a cheap,
 * version-stable "pressure" rate across PG 16–18 without a dedicated sampler
 * is easy to misread. Prefer WAL byte-rate from public stats for now.
 *
 * Classification: LOW / NORMAL / ELEVATED / CRITICAL (no predictive WAL).
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "pgstat.h"
#include "utils/timestamp.h"

#include "runtime_state.h"
#include "shmem.h"

const char *
pg_circuit_wal_pressure_name(int level)
{
	switch (level)
	{
		case PGC_WAL_CRITICAL:
			return "critical";
		case PGC_WAL_ELEVATED:
			return "elevated";
		case PGC_WAL_NORMAL:
			return "normal";
		case PGC_WAL_LOW:
		default:
			return "low";
	}
}

int
pg_circuit_pressure_score_wal(double bytes_per_sec)
{
	if (bytes_per_sec >= (double) pg_circuit_wal_pressure_high_bps)
		return 30;
	if (bytes_per_sec >= (double) pg_circuit_wal_pressure_elevated_bps)
		return 15;
	return 0;
}

void
pg_circuit_collect_wal_signals(PgCircuitRuntimeState *state)
{
	PgStat_WalStats *ws;
	double		bps = 0;
	int			level = PGC_WAL_LOW;

	state->wal_bytes_per_sec = 0;
	state->wal_pressure_level = PGC_WAL_LOW;

	ws = pgstat_fetch_stat_wal();
	if (ws == NULL)
		return;

	pg_circuit_wal_sample_update(ws->wal_bytes, GetCurrentTimestamp(),
								 &bps, &level);
	state->wal_bytes_per_sec = bps;
	state->wal_pressure_level = level;
}
