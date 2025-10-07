/*-------------------------------------------------------------------------
 *
 * blocker_graph.c
 *		Build blocker edges from the lock manager snapshot
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "miscadmin.h"
#include "storage/lock.h"
#include "utils/backend_status.h"
#include "utils/timestamp.h"

#include "blocker_graph.h"
#include "pg_circuit.h"
#include "relations.h"

#define PGC_MAX_BLOCKER_EDGES 256
#define PGC_MAX_BLOCKER_PIDS 64

static void compute_chain_severity(PgCircuitBlockerGraph *graph);

void
pg_circuit_blocker_graph_init(PgCircuitBlockerGraph *graph)
{
	MemSet(graph, 0, sizeof(*graph));
	graph->edges = NIL;
}

void
pg_circuit_blocker_graph_free(PgCircuitBlockerGraph *graph)
{
	ListCell   *lc;

	foreach(lc, graph->edges)
		pfree(lfirst(lc));
	list_free(graph->edges);
	graph->edges = NIL;
}

static bool
locktags_equal(const LOCKTAG *a, const LOCKTAG *b)
{
	return memcmp(a, b, sizeof(LOCKTAG)) == 0;
}

static double
pid_xact_age_seconds(int pid, TimestampTz now)
{
	int			i;
	int			n;

	n = pgstat_fetch_stat_numbackends();
	for (i = 1; i <= n; i++)
	{
		LocalPgBackendStatus *local_beentry;
		PgBackendStatus *beentry;

		local_beentry = pgstat_get_local_beentry_by_index(i);
		if (local_beentry == NULL)
			continue;
		beentry = &local_beentry->backendStatus;
		if (beentry->st_procpid != pid)
			continue;
		if (beentry->st_xact_start_timestamp == 0)
			return 0;
		return (double) (now - beentry->st_xact_start_timestamp) /
			(double) USECS_PER_SEC;
	}
	return 0;
}

static void
fill_relation_from_locktag(const LOCKTAG *tag, PgCircuitBlockerEdge *edge)
{
	edge->database_oid = InvalidOid;
	edge->relation_oid = InvalidOid;
	edge->relation_name[0] = '\0';

	if (tag->locktag_type == LOCKTAG_RELATION ||
		tag->locktag_type == LOCKTAG_RELATION_EXTEND ||
		tag->locktag_type == LOCKTAG_PAGE ||
		tag->locktag_type == LOCKTAG_TUPLE)
	{
		edge->database_oid = (Oid) tag->locktag_field1;
		edge->relation_oid = (Oid) tag->locktag_field2;
		if (OidIsValid(edge->relation_oid))
			pg_circuit_relation_name(edge->relation_oid,
									 edge->relation_name,
									 sizeof(edge->relation_name));
	}
}

static int
pid_index(int *pids, int npids, int pid)
{
	int			k;

	for (k = 0; k < npids; k++)
	{
		if (pids[k] == pid)
			return k;
	}
	return -1;
}

/*
 * Detect cycles in the blocker digraph (blocked -> blocking).
 */
static bool
graph_has_cycle(List *edges)
{
	int			pids[PGC_MAX_BLOCKER_PIDS];
	bool		adj[PGC_MAX_BLOCKER_PIDS][PGC_MAX_BLOCKER_PIDS];
	int			npids = 0;
	ListCell   *lc;
	int			i;
	int			j;
	int			k;

	MemSet(adj, 0, sizeof(adj));

	foreach(lc, edges)
	{
		PgCircuitBlockerEdge *e = (PgCircuitBlockerEdge *) lfirst(lc);
		int			a;
		int			b;

		a = pid_index(pids, npids, e->blocked_pid);
		if (a < 0)
		{
			if (npids >= PGC_MAX_BLOCKER_PIDS)
				continue;
			a = npids;
			pids[npids++] = e->blocked_pid;
		}
		b = pid_index(pids, npids, e->blocking_pid);
		if (b < 0)
		{
			if (npids >= PGC_MAX_BLOCKER_PIDS)
				continue;
			b = npids;
			pids[npids++] = e->blocking_pid;
		}
		adj[a][b] = true;
	}

	/* Warshall reachability; cycle if any node reaches itself */
	for (k = 0; k < npids; k++)
		for (i = 0; i < npids; i++)
			if (adj[i][k])
				for (j = 0; j < npids; j++)
					if (adj[k][j])
						adj[i][j] = true;

	for (i = 0; i < npids; i++)
		if (adj[i][i])
			return true;
	return false;
}

void
pg_circuit_build_blocker_graph(PgCircuitBlockerGraph *graph)
{
	LockData   *lockData;
	TimestampTz now = GetCurrentTimestamp();
	int			i;
	int			j;
	int			blocked_pids[PGC_MAX_BLOCKER_EDGES];
	int			blocking_pids[PGC_MAX_BLOCKER_EDGES];
	int			nblocked = 0;
	int			nblocking = 0;

	pg_circuit_blocker_graph_init(graph);

	lockData = GetLockStatusData();
	if (lockData == NULL)
		return;

	for (i = 0; i < lockData->nelements; i++)
	{
		LockInstanceData *waiter = &lockData->locks[i];
		double		blocked_for;
		LOCKMODE	awaited;

		if (waiter->pid <= 0)
			continue;
		awaited = waiter->waitLockMode;
		if (awaited == NoLock)
			continue;
		if (waiter->pid == MyProcPid)
			continue;

		blocked_for = 0;
		if (waiter->waitStart > 0)
		{
			blocked_for = (double) (now - waiter->waitStart) /
				(double) USECS_PER_SEC;
			if (blocked_for < 0)
				blocked_for = 0;
		}
		if (blocked_for > graph->max_blocked_for_seconds)
			graph->max_blocked_for_seconds = blocked_for;

		for (j = 0; j < lockData->nelements; j++)
		{
			LockInstanceData *holder = &lockData->locks[j];
			PgCircuitBlockerEdge *edge;
			LOCKMODE	mode;
			bool		conflict = false;
			int			k;
			bool		seen;

			if (holder->pid <= 0 || holder->pid == waiter->pid)
				continue;
			if (holder->holdMask == 0)
				continue;
			if (!locktags_equal(&waiter->locktag, &holder->locktag))
				continue;

			for (mode = 1; mode < MAX_LOCKMODES; mode++)
			{
				if ((holder->holdMask & LOCKBIT_ON(mode)) != 0 &&
					DoLockModesConflict(mode, awaited))
				{
					conflict = true;
					break;
				}
			}
			if (!conflict)
				continue;

			if (list_length(graph->edges) >= PGC_MAX_BLOCKER_EDGES)
				goto done_scan;

			edge = palloc0(sizeof(PgCircuitBlockerEdge));
			edge->blocked_pid = waiter->pid;
			edge->blocking_pid = holder->pid;
			edge->blocked_for_seconds = blocked_for;
			edge->blocking_transaction_age_seconds =
				pid_xact_age_seconds(holder->pid, now);
			fill_relation_from_locktag(&waiter->locktag, edge);
			if (waiter->locktag.locktag_type < LOCKTAG_LAST_TYPE)
				strlcpy(edge->lock_type,
						LockTagTypeNames[waiter->locktag.locktag_type],
						sizeof(edge->lock_type));
			else
				strlcpy(edge->lock_type, "?", sizeof(edge->lock_type));
			strlcpy(edge->lock_mode,
					GetLockmodeName(DEFAULT_LOCKMETHOD, awaited),
					sizeof(edge->lock_mode));
			graph->edges = lappend(graph->edges, edge);

			seen = false;
			for (k = 0; k < nblocked; k++)
				if (blocked_pids[k] == waiter->pid)
					seen = true;
			if (!seen && nblocked < PGC_MAX_BLOCKER_EDGES)
				blocked_pids[nblocked++] = waiter->pid;

			seen = false;
			for (k = 0; k < nblocking; k++)
				if (blocking_pids[k] == holder->pid)
					seen = true;
			if (!seen && nblocking < PGC_MAX_BLOCKER_EDGES)
				blocking_pids[nblocking++] = holder->pid;
		}
	}

done_scan:
	graph->blocked_sessions = nblocked;
	graph->blocking_sessions = nblocking;
	graph->has_cycle = graph_has_cycle(graph->edges);
	compute_chain_severity(graph);

	pfree(lockData->locks);
	pfree(lockData);
}

/*
 * From each root blocker (a PID that blocks someone and is not itself
 * blocked), compute descendant count and depth along blocked←blocking edges.
 * Edge direction in our graph is blocked_pid → blocking_pid; for descendants
 * we invert: from blocker, follow edges where blocking_pid == blocker.
 */
static void
compute_chain_severity(PgCircuitBlockerGraph *graph)
{
	int			pids[PGC_MAX_BLOCKER_PIDS];
	bool		is_blocked[PGC_MAX_BLOCKER_PIDS];
	bool		is_blocker[PGC_MAX_BLOCKER_PIDS];
	int			npids = 0;
	ListCell   *lc;
	int			i;

	MemSet(is_blocked, 0, sizeof(is_blocked));
	MemSet(is_blocker, 0, sizeof(is_blocker));
	graph->max_chain_depth = 0;
	graph->max_descendant_count = 0;

	foreach(lc, graph->edges)
	{
		PgCircuitBlockerEdge *e = (PgCircuitBlockerEdge *) lfirst(lc);
		int			a;
		int			b;

		a = pid_index(pids, npids, e->blocked_pid);
		if (a < 0)
		{
			if (npids >= PGC_MAX_BLOCKER_PIDS)
				continue;
			a = npids;
			pids[npids++] = e->blocked_pid;
		}
		b = pid_index(pids, npids, e->blocking_pid);
		if (b < 0)
		{
			if (npids >= PGC_MAX_BLOCKER_PIDS)
				continue;
			b = npids;
			pids[npids++] = e->blocking_pid;
		}
		is_blocked[a] = true;
		is_blocker[b] = true;
	}

	for (i = 0; i < npids; i++)
	{
		bool		visited[PGC_MAX_BLOCKER_PIDS];
		int			queue[PGC_MAX_BLOCKER_PIDS];
		int			depth[PGC_MAX_BLOCKER_PIDS];
		int			qh = 0;
		int			qt = 0;
		int			descendants = 0;
		int			local_max_depth = 0;
		ListCell   *lc2;

		/* Root = blocks someone and is not waiting */
		if (!is_blocker[i] || is_blocked[i])
			continue;

		MemSet(visited, 0, sizeof(visited));
		queue[qt] = i;
		depth[i] = 0;
		visited[i] = true;
		qt++;

		while (qh < qt)
		{
			int			cur = queue[qh++];

			foreach(lc2, graph->edges)
			{
				PgCircuitBlockerEdge *e = (PgCircuitBlockerEdge *) lfirst(lc2);
				int			child;

				if (e->blocking_pid != pids[cur])
					continue;
				child = pid_index(pids, npids, e->blocked_pid);
				if (child < 0 || visited[child])
					continue;
				visited[child] = true;
				depth[child] = depth[cur] + 1;
				if (depth[child] > local_max_depth)
					local_max_depth = depth[child];
				descendants++;
				if (qt < PGC_MAX_BLOCKER_PIDS)
					queue[qt++] = child;
			}
		}

		if (descendants > graph->max_descendant_count)
			graph->max_descendant_count = descendants;
		if (local_max_depth > graph->max_chain_depth)
			graph->max_chain_depth = local_max_depth;
	}
}
