/*-------------------------------------------------------------------------
 *
 * relations.c
 *		Relation metadata helpers for PG Circuit
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "access/relation.h"
#include "access/tableam.h"
#include "catalog/namespace.h"
#include "storage/bufmgr.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"

#include "pg_circuit.h"
#include "relations.h"

void
pg_circuit_relation_name(Oid relid, char *buf, size_t buflen)
{
	char	   *name;

	if (!OidIsValid(relid))
	{
		strlcpy(buf, "?", buflen);
		return;
	}

	name = get_rel_name(relid);
	if (name == NULL)
	{
		strlcpy(buf, "?", buflen);
		return;
	}

	strlcpy(buf, name, buflen);
	pfree(name);
}

/*
 * schema.relname when available. Falls back to bare name or "?".
 */
void
pg_circuit_relation_qualname(Oid relid, char *buf, size_t buflen)
{
	char	   *nsp;
	char	   *rel;

	if (!OidIsValid(relid))
	{
		strlcpy(buf, "?", buflen);
		return;
	}

	nsp = get_namespace_name(get_rel_namespace(relid));
	rel = get_rel_name(relid);
	if (rel == NULL)
	{
		if (nsp)
			pfree(nsp);
		strlcpy(buf, "?", buflen);
		return;
	}

	if (nsp != NULL)
		snprintf(buf, buflen, "%s.%s", nsp, rel);
	else
		strlcpy(buf, rel, buflen);

	if (nsp)
		pfree(nsp);
	pfree(rel);
}

/*
 * Best-effort pg_class.reltuples. Returns -1 when unavailable.
 */
double
pg_circuit_relation_estimated_tuples(Oid relid)
{
	Relation	rel;
	double		tuples;

	if (!OidIsValid(relid))
		return -1;

	rel = try_relation_open(relid, AccessShareLock);
	if (rel == NULL)
		return -1;

	tuples = (double) rel->rd_rel->reltuples;
	relation_close(rel, AccessShareLock);

	if (tuples < 0)
		return -1;
	return tuples;
}

/*
 * Upper bound on live heap tuples from on-disk page count. Used to clamp
 * absurd planner/reltuples estimates on tiny heaps. Returns -1 if unknown.
 */
double
pg_circuit_relation_physical_tuple_bound(Oid relid)
{
	Relation	rel;
	BlockNumber pages;
	double		bound;

	if (!OidIsValid(relid))
		return -1;

	rel = try_relation_open(relid, AccessShareLock);
	if (rel == NULL)
		return -1;

	pages = RelationGetNumberOfBlocks(rel);
	relation_close(rel, AccessShareLock);

	if (pages == 0)
		return 0;

	/* MaxHeapTuplesPerPage is optimistic; still a useful ceiling. */
	bound = (double) pages * (double) MaxHeapTuplesPerPage;
	return bound;
}

/*
 * Best estimate of table cardinality for coverage checks: prefer catalog
 * reltuples, but never trust it above the physical page-based ceiling.
 */
double
pg_circuit_relation_table_rows_estimate(Oid relid)
{
	double		tuples;
	double		bound;

	tuples = pg_circuit_relation_estimated_tuples(relid);
	bound = pg_circuit_relation_physical_tuple_bound(relid);

	if (tuples > 0 && bound >= 0 && tuples > bound)
		return bound;
	if (tuples > 0)
		return tuples;
	if (bound > 0)
		return bound;
	return -1;
}

/*
 * Best-effort relation size in bytes (main fork). Returns -1 when unavailable.
 */
int64
pg_circuit_relation_size(Oid relid)
{
	Relation	rel;
	int64		size;

	if (!OidIsValid(relid))
		return -1;

	rel = try_relation_open(relid, AccessShareLock);
	if (rel == NULL)
		return -1;

	size = (int64) RelationGetNumberOfBlocks(rel) * BLCKSZ;
	relation_close(rel, AccessShareLock);
	return size;
}
