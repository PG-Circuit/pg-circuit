/*-------------------------------------------------------------------------
 *
 * replication.c
 *		Replication lag signals (write / flush / replay + reply age)
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "miscadmin.h"
#include "replication/walsender.h"
#include "replication/walsender_private.h"
#include "storage/lwlock.h"
#include "storage/spin.h"
#include "utils/timestamp.h"

#include "pg_circuit.h"
#include "fault.h"
#include "runtime_state.h"

static double
lag_offset_to_seconds(TimeOffset lag)
{
	/* Measured lag times use -1 for unknown/none. */
	if (lag < 0)
		return -1.0;
	return (double) lag / (double) USECS_PER_SEC;
}

static void
bump_max(double *dst, double v)
{
	if (v < 0)
		return;
	if (*dst < 0 || v > *dst)
		*dst = v;
}

/*
 * Best-effort replication signals across connected WAL senders.
 *
 * Limitations:
 * - Requires max_wal_senders > 0 and connected replicas.
 * - No replicas => health NONE, lag 0 (not an error, not pressure).
 * - writeLag/flushLag/applyLag may be -1 if standby has not reported yet;
 *   replyTime age is used as a conservative fallback for scoring.
 * - Relies on replication/walsender_private.h (same process, native extension).
 */
void
pg_circuit_collect_replication_signals(PgCircuitRuntimeState *state)
{
	int			i;
	double		max_write = -1.0;
	double		max_flush = -1.0;
	double		max_replay = -1.0;
	double		max_reply = 0.0;
	double		effective = 0.0;
	TimestampTz now;
	int			replicas = 0;
	int			lag_warn;
	int			lag_crit;

	state->replica_count = 0;
	state->max_replication_lag_seconds = 0.0;
	state->max_write_lag_seconds = -1;
	state->max_flush_lag_seconds = -1;
	state->max_replay_lag_seconds = -1;
	state->max_reply_lag_seconds = 0;
	state->replication_health = PGC_REPL_NONE;

	if (pg_circuit_fault_is("replication"))
		elog(ERROR, "injected replication-stat failure");

	if (max_wal_senders <= 0 || WalSndCtl == NULL)
		return;

	now = GetCurrentTimestamp();
	lag_warn = pg_circuit_effective_lag_warn_seconds();
	lag_crit = pg_circuit_effective_lag_critical_seconds();

	LWLockAcquire(SyncRepLock, LW_SHARED);

	for (i = 0; i < max_wal_senders; i++)
	{
		WalSnd	   *walsnd = &WalSndCtl->walsnds[i];
		TimestampTz reply;
		TimeOffset	writeLag;
		TimeOffset	flushLag;
		TimeOffset	applyLag;
		double		reply_sec = 0.0;

		SpinLockAcquire(&walsnd->mutex);
		if (walsnd->pid == 0)
		{
			SpinLockRelease(&walsnd->mutex);
			continue;
		}
		reply = walsnd->replyTime;
		writeLag = walsnd->writeLag;
		flushLag = walsnd->flushLag;
		applyLag = walsnd->applyLag;
		SpinLockRelease(&walsnd->mutex);

		replicas++;

		bump_max(&max_write, lag_offset_to_seconds(writeLag));
		bump_max(&max_flush, lag_offset_to_seconds(flushLag));
		bump_max(&max_replay, lag_offset_to_seconds(applyLag));

		if (reply > 0)
		{
			reply_sec = (double) (now - reply) / (double) USECS_PER_SEC;
			if (reply_sec < 0)
				reply_sec = 0;
			if (reply_sec > max_reply)
				max_reply = reply_sec;
		}
	}

	LWLockRelease(SyncRepLock);

	state->replica_count = replicas;
	state->max_write_lag_seconds = max_write;
	state->max_flush_lag_seconds = max_flush;
	state->max_replay_lag_seconds = max_replay;
	state->max_reply_lag_seconds = max_reply;

	/* Effective lag for scoring: max of known write/flush/replay, else reply */
	effective = -1.0;
	bump_max(&effective, max_write);
	bump_max(&effective, max_flush);
	bump_max(&effective, max_replay);
	if (effective < 0)
		effective = max_reply;
	else if (max_reply > effective)
		effective = max_reply;

	if (effective < 0)
		effective = 0;
	state->max_replication_lag_seconds = effective;

	if (replicas <= 0)
	{
		state->replication_health = PGC_REPL_NONE;
		return;
	}

	if (lag_crit > 0 && effective >= (double) lag_crit)
		state->replication_health = PGC_REPL_SEVERE;
	else if (lag_warn > 0 && effective >= (double) lag_warn)
		state->replication_health = PGC_REPL_LAGGING;
	else
		state->replication_health = PGC_REPL_HEALTHY;
}
