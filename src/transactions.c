/*-------------------------------------------------------------------------
 *
 * transactions.c
 *		Transaction and connection pressure signals
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "miscadmin.h"
#include "postmaster/postmaster.h"
#include "utils/backend_status.h"
#include "utils/timestamp.h"

#include "pg_circuit.h"
#include "runtime_state.h"

/*
 * Scan backend activity for transaction-related pressure signals.
 *
 * Limitations:
 * - Uses pgstat backend activity snapshot (best-effort, not lock-free exact).
 * - Excludes the current backend so self-inspection does not inflate pressure.
 * - Counts only client backends (B_BACKEND); background workers are ignored.
 */
void
pg_circuit_collect_transaction_signals(PgCircuitRuntimeState *state)
{
	int			i;
	int			num_backends;
	TimestampTz now = GetCurrentTimestamp();
	int64		long_limit_us =
		(int64) pg_circuit_long_transaction_seconds * USECS_PER_SEC;

	state->active_connections = 0;
	state->idle_connections = 0;
	state->active_query_backends = 0;
	state->active_transactions = 0;
	state->long_transactions = 0;
	state->idle_in_transaction = 0;
	state->oldest_transaction_age_seconds = 0;
	state->oldest_idle_in_transaction_age_seconds = 0;
	state->max_connections = MaxConnections;
	state->reserved_connections = SuperuserReservedConnections;

	num_backends = pgstat_fetch_stat_numbackends();

	for (i = 1; i <= num_backends; i++)
	{
		LocalPgBackendStatus *local_beentry;
		PgBackendStatus *beentry;
		TimestampTz xact_start;
		int64		age_us;
		double		age_sec;

		local_beentry = pgstat_get_local_beentry_by_index(i);
		if (local_beentry == NULL)
			continue;

		beentry = &local_beentry->backendStatus;
		if (beentry->st_procpid <= 0)
			continue;

		/* Do not count ourselves as pressure */
		if (beentry->st_procpid == MyProcPid)
			continue;

		/* Application backends only */
		if (beentry->st_backendType != B_BACKEND)
			continue;

		state->active_connections++;

		if (beentry->st_state == STATE_IDLE)
			state->idle_connections++;
		else if (beentry->st_state == STATE_RUNNING)
			state->active_query_backends++;

		if (beentry->st_state == STATE_UNDEFINED ||
			beentry->st_state == STATE_IDLE)
			continue;

		if (beentry->st_xact_start_timestamp == 0)
			continue;

		state->active_transactions++;

		xact_start = beentry->st_xact_start_timestamp;
		age_us = now - xact_start;
		if (age_us < 0)
			age_us = 0;
		age_sec = (double) age_us / (double) USECS_PER_SEC;

		if (age_sec > state->oldest_transaction_age_seconds)
			state->oldest_transaction_age_seconds = age_sec;

		if (beentry->st_state == STATE_IDLEINTRANSACTION ||
			beentry->st_state == STATE_IDLEINTRANSACTION_ABORTED)
		{
			state->idle_in_transaction++;
			if (age_sec > state->oldest_idle_in_transaction_age_seconds)
				state->oldest_idle_in_transaction_age_seconds = age_sec;
		}

		if (age_us >= long_limit_us)
			state->long_transactions++;
	}
}
