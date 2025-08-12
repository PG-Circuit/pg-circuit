/*-------------------------------------------------------------------------
 *
 * locks.c
 *		Lock wait / blocked-session signals via blocker graph
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "blocker_graph.h"
#include "pg_circuit.h"
#include "runtime_state.h"

/*
 * Populate lock-pressure fields from the blocker graph.
 *
 * Limitations:
 * - Snapshot is best-effort; races with lock acquire/release.
 * - Excludes the current backend from edge construction.
 * - Chain depth/descendants are capped by PGC_MAX_BLOCKER_PIDS.
 */
void
pg_circuit_collect_lock_signals(PgCircuitRuntimeState *state)
{
	PgCircuitBlockerGraph graph;

	state->blocked_sessions = 0;
	state->blocking_sessions = 0;
	state->max_lock_wait_seconds = 0;
	state->lock_cycle_detected = false;
	state->max_lock_chain_depth = 0;
	state->max_lock_descendants = 0;

	pg_circuit_build_blocker_graph(&graph);

	state->blocked_sessions = graph.blocked_sessions;
	state->blocking_sessions = graph.blocking_sessions;
	state->max_lock_wait_seconds = graph.max_blocked_for_seconds;
	state->lock_cycle_detected = graph.has_cycle;
	state->max_lock_chain_depth = graph.max_chain_depth;
	state->max_lock_descendants = graph.max_descendant_count;

	pg_circuit_blocker_graph_free(&graph);
}
