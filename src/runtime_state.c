/*-------------------------------------------------------------------------
 *
 * runtime_state.c
 *		Collect and resolve runtime safety mode
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "lib/stringinfo.h"

#include "pg_circuit.h"
#include "runtime_state.h"

/*
 * Pressure factor: long / idle-in-transaction backends.
 * Each long tx contributes 12; each idle-in-xact contributes 6.
 */
int
pg_circuit_pressure_score_transactions(int long_transactions,
									   int idle_in_transaction)
{
	return pg_circuit_pressure_score_transactions_ex(long_transactions,
													 idle_in_transaction,
													 0.0, 0);
}

int
pg_circuit_pressure_score_transactions_ex(int long_transactions,
										  int idle_in_transaction,
										  double oldest_age_seconds,
										  int long_limit_seconds)
{
	int			score = 0;

	if (long_transactions > 0)
		score += long_transactions * 12;
	if (idle_in_transaction > 0)
		score += idle_in_transaction * 6;

	/*
	 * Very old open transactions worsen bloat/cleanup impact beyond the
	 * simple count. Extra bump only when age exceeds 2× the long-tx limit.
	 */
	if (long_limit_seconds > 0 &&
		oldest_age_seconds >= (double) long_limit_seconds * 2.0)
		score += 15;
	else if (long_limit_seconds > 0 &&
			 oldest_age_seconds >= (double) long_limit_seconds)
		score += 5;

	return score;
}

int
pg_circuit_pressure_score_locks(int blocked_sessions,
								int blocking_sessions)
{
	return pg_circuit_pressure_score_locks_ex(blocked_sessions,
											  blocking_sessions,
											  0, 0, 0.0);
}

/*
 * Pressure factor: lock waiters + chain severity.
 * A root blocker with many descendants is worse than a single edge.
 */
int
pg_circuit_pressure_score_locks_ex(int blocked_sessions,
								   int blocking_sessions,
								   int max_chain_depth,
								   int max_descendants,
								   double max_wait_seconds)
{
	int			score = 0;

	if (blocked_sessions >= 1)
		score += 8;
	if (blocked_sessions >= 5)
		score += 15;
	if (blocked_sessions >= 20)
		score += 25;
	if (blocking_sessions > 0)
		score += Min(blocking_sessions, 5) * 4;

	if (max_descendants >= 50)
		score += 25;
	else if (max_descendants >= 20)
		score += 15;
	else if (max_descendants >= 5)
		score += 8;
	else if (max_descendants >= 2)
		score += 4;

	if (max_chain_depth >= 5)
		score += 12;
	else if (max_chain_depth >= 3)
		score += 8;
	else if (max_chain_depth >= 2)
		score += 3;

	if (max_wait_seconds >= 30.0)
		score += 8;
	else if (max_wait_seconds >= 5.0)
		score += 4;

	return score;
}

/*
 * Pressure factor: replication lag.
 * No replicas / zero lag contributes nothing (not an error).
 */
int
pg_circuit_pressure_score_replication(double lag_seconds,
									  int warn_seconds,
									  int critical_seconds)
{
	if (lag_seconds <= 0)
		return 0;
	if (critical_seconds > 0 && lag_seconds >= (double) critical_seconds)
		return 35;
	if (warn_seconds > 0 && lag_seconds >= (double) warn_seconds)
		return 18;
	return 0;
}

/*
 * Connection pressure: headroom vs MaxConnections, not a raw ratio alone.
 * Reserved superuser slots are excluded from usable capacity.
 */
int
pg_circuit_pressure_score_connections(int used_connections,
									  int max_connections,
									  int reserved_connections,
									  int idle_in_transaction,
									  int active_query_backends)
{
	int			usable;
	int			score = 0;
	double		ratio;

	if (max_connections <= 0)
		return 0;

	usable = max_connections - reserved_connections;
	if (usable < 1)
		usable = 1;

	ratio = (double) used_connections / (double) usable;

	if (ratio >= 0.95)
		score += 25;
	else if (ratio >= 0.85)
		score += 15;
	else if (ratio >= 0.70)
		score += 8;

	if (idle_in_transaction > 0)
		score += Min(idle_in_transaction, 8) * 2;

	if (active_query_backends > 0 &&
		active_query_backends >= (usable / 2))
		score += 5;

	return score;
}

int
pg_circuit_pressure_score_combine(int tx_score,
								  int lock_score,
								  int repl_score,
								  int wal_score)
{
	return pg_circuit_pressure_score_combine5(tx_score, lock_score,
											  repl_score, wal_score, 0);
}

int
pg_circuit_pressure_score_combine5(int tx_score,
								   int lock_score,
								   int repl_score,
								   int wal_score,
								   int conn_score)
{
	int			score = tx_score + lock_score + repl_score + wal_score + conn_score;

	if (score < 0)
		return 0;
	if (score > 100)
		return 100;
	return score;
}

void
pg_circuit_effective_risk_thresholds(int *warn_at, int *block_at)
{
	*warn_at = pg_circuit_risk_warn_threshold;
	*block_at = pg_circuit_risk_block_threshold;
}

int
pg_circuit_effective_lag_warn_seconds(void)
{
	return pg_circuit_replication_lag_warning_seconds;
}

int
pg_circuit_effective_lag_critical_seconds(void)
{
	return pg_circuit_replication_lag_critical_seconds;
}

void
pg_circuit_compute_pressure_and_mode(PgCircuitRuntimeState *state)
{
	int			tx;
	int			locks;
	int			repl;
	int			wal;
	int			conn;
	int			lag_warn;
	int			lag_crit;

	lag_warn = pg_circuit_effective_lag_warn_seconds();
	lag_crit = pg_circuit_effective_lag_critical_seconds();

	tx = pg_circuit_pressure_score_transactions_ex(state->long_transactions,
												   state->idle_in_transaction,
												   state->oldest_transaction_age_seconds,
												   pg_circuit_long_transaction_seconds);
	locks = pg_circuit_pressure_score_locks_ex(state->blocked_sessions,
											   state->blocking_sessions,
											   state->max_lock_chain_depth,
											   state->max_lock_descendants,
											   state->max_lock_wait_seconds);
	repl = pg_circuit_pressure_score_replication(state->max_replication_lag_seconds,
												 lag_warn,
												 lag_crit);
	wal = pg_circuit_pressure_score_wal(state->wal_bytes_per_sec);
	conn = pg_circuit_pressure_score_connections(state->active_connections,
												 state->max_connections,
												 state->reserved_connections,
												 state->idle_in_transaction,
												 state->active_query_backends);

	state->pressure_tx = tx;
	state->pressure_locks = locks;
	state->pressure_replication = repl;
	state->pressure_wal = wal;
	state->pressure_connections = conn;
	state->connection_pressure_score = conn;

	state->pressure_score = pg_circuit_pressure_score_combine5(tx, locks, repl, wal, conn);
	state->mode = pg_circuit_resolve_safety_mode(state);
}

void
pg_circuit_collect_runtime_state(PgCircuitRuntimeState *state)
{
	MemSet(state, 0, sizeof(*state));

	/* Defaults for lag fields that use -1 as unknown in collectors */
	state->max_write_lag_seconds = -1;
	state->max_flush_lag_seconds = -1;
	state->max_replay_lag_seconds = -1;

	pg_circuit_collect_transaction_signals(state);
	pg_circuit_collect_lock_signals(state);
	pg_circuit_collect_replication_signals(state);
	pg_circuit_collect_wal_signals(state);
	pg_circuit_compute_pressure_and_mode(state);
}

/*
 * Community: always NORMAL. Pressure signals are still collected for display.
 */
PgCircuitSafetyMode
pg_circuit_resolve_safety_mode(const PgCircuitRuntimeState *state)
{
	(void) state;
	(void) pg_circuit_runtime_mode;
	return PGC_SAFETY_NORMAL;
}

const char *
pg_circuit_replication_health_name(PgCircuitReplicationHealth h)
{
	switch (h)
	{
		case PGC_REPL_HEALTHY:
			return "healthy";
		case PGC_REPL_LAGGING:
			return "lagging";
		case PGC_REPL_SEVERE:
			return "severely_lagging";
		case PGC_REPL_NONE:
		default:
			return "none";
	}
}

char *
pg_circuit_format_pressure_contributors(const PgCircuitRuntimeState *state)
{
	StringInfoData buf;
	typedef struct
	{
		const char *name;
		int			contrib;
	} Contrib;
	Contrib		parts[5];
	int			n = 0;
	int			i;
	int			j;

	parts[n].name = "transactions";
	parts[n++].contrib = state->pressure_tx;
	parts[n].name = "locks";
	parts[n++].contrib = state->pressure_locks;
	parts[n].name = "replication_lag";
	parts[n++].contrib = state->pressure_replication;
	parts[n].name = "wal";
	parts[n++].contrib = state->pressure_wal;
	parts[n].name = "connection_pressure";
	parts[n++].contrib = state->pressure_connections;

	for (i = 1; i < n; i++)
	{
		Contrib		tmp = parts[i];

		j = i;
		while (j > 0 && parts[j - 1].contrib < tmp.contrib)
		{
			parts[j] = parts[j - 1];
			j--;
		}
		parts[j] = tmp;
	}

	initStringInfo(&buf);
	appendStringInfo(&buf, "effective_mode: %s\n",
					 pg_circuit_safety_mode_name(state->mode));
	appendStringInfo(&buf, "pressure_score: %d\n", state->pressure_score);
	appendStringInfoString(&buf, "contributors:\n");
	for (i = 0; i < n; i++)
	{
		if (parts[i].contrib <= 0)
			continue;
		appendStringInfo(&buf, "  %s +%d\n", parts[i].name, parts[i].contrib);
	}
	return buf.data;
}
