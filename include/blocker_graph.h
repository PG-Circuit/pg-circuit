/*-------------------------------------------------------------------------
 *
 * blocker_graph.h
 *		Lock wait / blocker graph for PG Circuit
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_CIRCUIT_BLOCKER_GRAPH_H
#define PG_CIRCUIT_BLOCKER_GRAPH_H

#include "postgres.h"

#include "nodes/pg_list.h"

typedef struct PgCircuitBlockerEdge
{
	int			blocked_pid;
	int			blocking_pid;
	double		blocked_for_seconds;
	double		blocking_transaction_age_seconds;
	Oid			database_oid;
	Oid			relation_oid;
	char		relation_name[NAMEDATALEN];
	char		lock_type[32];
	char		lock_mode[32];
} PgCircuitBlockerEdge;

typedef struct PgCircuitBlockerGraph
{
	List	   *edges;			/* List of PgCircuitBlockerEdge* */
	int			blocked_sessions;
	int			blocking_sessions;
	double		max_blocked_for_seconds;
	bool		has_cycle;
	int			max_chain_depth;		/* longest blocker→blocked path */
	int			max_descendant_count;	/* max backends reachable from one root */
} PgCircuitBlockerGraph;

extern void pg_circuit_blocker_graph_init(PgCircuitBlockerGraph *graph);
extern void pg_circuit_blocker_graph_free(PgCircuitBlockerGraph *graph);
extern void pg_circuit_build_blocker_graph(PgCircuitBlockerGraph *graph);

#endif							/* PG_CIRCUIT_BLOCKER_GRAPH_H */
