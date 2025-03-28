/*-------------------------------------------------------------------------
 *
 * runtime_state.h
 *		Compact runtime-state model for PG Circuit
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_CIRCUIT_RUNTIME_STATE_H
#define PG_CIRCUIT_RUNTIME_STATE_H

#include "pg_circuit.h"

/* WAL pressure tiers (deterministic rate classification) */
#define PGC_WAL_LOW			0
#define PGC_WAL_NORMAL		1
#define PGC_WAL_ELEVATED	2
#define PGC_WAL_CRITICAL	3

/* Replication health */
typedef enum PgCircuitReplicationHealth
{
	PGC_REPL_NONE = 0,			/* no connected replicas */
	PGC_REPL_HEALTHY,
	PGC_REPL_LAGGING,
	PGC_REPL_SEVERE
} PgCircuitReplicationHealth;

typedef struct PgCircuitRuntimeState
{
	PgCircuitSafetyMode mode;

	/* Connections */
	int			active_connections; /* client backends excl. self */
	int			idle_connections;
	int			active_query_backends;	/* STATE_RUNNING */
	int			max_connections;
	int			reserved_connections;
	int			connection_pressure_score;

	/* Transactions */
	int			active_transactions;
	int			long_transactions;
	int			idle_in_transaction;
	double		oldest_transaction_age_seconds;
	double		oldest_idle_in_transaction_age_seconds;

	/* Locks / blocker chains */
	int			blocked_sessions;
	int			blocking_sessions;
	double		max_lock_wait_seconds;
	bool		lock_cycle_detected;
	int			max_lock_chain_depth;
	int			max_lock_descendants;

	/* Replication */
	int			replica_count;
	double		max_replication_lag_seconds;	/* effective max used for scoring */
	double		max_write_lag_seconds;			/* -1 unknown → stored as -1 */
	double		max_flush_lag_seconds;
	double		max_replay_lag_seconds;
	double		max_reply_lag_seconds;
	PgCircuitReplicationHealth replication_health;

	/* WAL */
	double		wal_bytes_per_sec;
	int			wal_pressure_level; /* PGC_WAL_* */

	/* Pressure */
	int			pressure_score;		/* 0-100 combined */
	int			pressure_tx;
	int			pressure_locks;
	int			pressure_replication;
	int			pressure_wal;
	int			pressure_connections;
} PgCircuitRuntimeState;

extern void pg_circuit_collect_runtime_state(PgCircuitRuntimeState *state);
extern void pg_circuit_compute_pressure_and_mode(PgCircuitRuntimeState *state);
extern PgCircuitSafetyMode pg_circuit_resolve_safety_mode(const PgCircuitRuntimeState *state);
extern void pg_circuit_collect_transaction_signals(PgCircuitRuntimeState *state);
extern void pg_circuit_collect_lock_signals(PgCircuitRuntimeState *state);
extern void pg_circuit_collect_replication_signals(PgCircuitRuntimeState *state);
extern void pg_circuit_collect_wal_signals(PgCircuitRuntimeState *state);

/* Independently testable pressure factors */
extern int	pg_circuit_pressure_score_transactions(int long_transactions,
												   int idle_in_transaction);
extern int	pg_circuit_pressure_score_transactions_ex(int long_transactions,
													  int idle_in_transaction,
													  double oldest_age_seconds,
													  int long_limit_seconds);
extern int	pg_circuit_pressure_score_locks(int blocked_sessions,
											int blocking_sessions);
extern int	pg_circuit_pressure_score_locks_ex(int blocked_sessions,
											   int blocking_sessions,
											   int max_chain_depth,
											   int max_descendants,
											   double max_wait_seconds);
extern int	pg_circuit_pressure_score_replication(double lag_seconds,
												  int warn_seconds,
												  int critical_seconds);
extern int	pg_circuit_pressure_score_wal(double bytes_per_sec);
extern int	pg_circuit_pressure_score_connections(int used_connections,
												  int max_connections,
												  int reserved_connections,
												  int idle_in_transaction,
												  int active_query_backends);
extern int	pg_circuit_pressure_score_combine(int tx_score,
											  int lock_score,
											  int repl_score,
											  int wal_score);
extern int	pg_circuit_pressure_score_combine5(int tx_score,
											   int lock_score,
											   int repl_score,
											   int wal_score,
											   int conn_score);
extern const char *pg_circuit_wal_pressure_name(int level);
extern const char *pg_circuit_replication_health_name(PgCircuitReplicationHealth h);
extern char *pg_circuit_format_pressure_contributors(const PgCircuitRuntimeState *state);

extern void pg_circuit_effective_risk_thresholds(int *warn_at, int *block_at);
extern int	pg_circuit_effective_lag_warn_seconds(void);
extern int	pg_circuit_effective_lag_critical_seconds(void);

#endif							/* PG_CIRCUIT_RUNTIME_STATE_H */
