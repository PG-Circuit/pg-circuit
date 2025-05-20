/*-------------------------------------------------------------------------
 *
 * query_analysis.c
 *		AST-based query classification (no regex) — Community
 *
 * WHERE-qualified DML stays OTHER (no DELETE_LARGE / UPDATE_LARGE).
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/relation.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/planner.h"
#include "parser/parsetree.h"
#include "utils/rel.h"

#include "pg_circuit.h"
#include "relations.h"
#include "risk_engine.h"

void		pg_circuit_analyze_query(Query *query, PlannedStmt *stmt,
									 PgCircuitQueryInfo *qinfo);
void		pg_circuit_analyze_plannedstmt(PlannedStmt *stmt,
										   PgCircuitQueryInfo *qinfo);

static void
init_qinfo(PgCircuitQueryInfo *qinfo)
{
	MemSet(qinfo, 0, sizeof(*qinfo));
	qinfo->kind = PGC_QKIND_OTHER;
	qinfo->relation_oid = InvalidOid;
	qinfo->estimated_rows = -1;
	qinfo->relation_size = -1;
	qinfo->has_where = false;
	qinfo->is_write = false;
}

static Oid
result_relation_oid(Query *query)
{
	RangeTblEntry *rte;

	if (query->resultRelation <= 0)
		return InvalidOid;
	if (query->resultRelation > list_length(query->rtable))
		return InvalidOid;

	rte = rt_fetch(query->resultRelation, query->rtable);
	if (rte == NULL || rte->rtekind != RTE_RELATION)
		return InvalidOid;
	return rte->relid;
}

static bool
query_has_where(Query *query)
{
	if (query->jointree == NULL)
		return false;
	return (query->jointree->quals != NULL);
}

static double
clamp_estimated_rows(Oid relid, double rows)
{
	double		bound;

	if (rows < 0)
		return rows;

	bound = pg_circuit_relation_physical_tuple_bound(relid);
	if (bound >= 0 && rows > bound)
		return bound;
	return rows;
}

static double
plan_estimated_rows(PlannedStmt *stmt)
{
	Plan	   *plan;

	if (stmt == NULL || stmt->planTree == NULL)
		return -1;

	plan = stmt->planTree;

	if (IsA(plan, ModifyTable))
	{
		ModifyTable *mt = (ModifyTable *) plan;

		if (mt->plan.lefttree != NULL && mt->plan.lefttree->plan_rows > 0)
			return mt->plan.lefttree->plan_rows;
		if (mt->plan.plan_rows > 0)
			return mt->plan.plan_rows;
	}

	if (plan->plan_rows > 0)
		return plan->plan_rows;

	return -1;
}

void
pg_circuit_analyze_query(Query *query, PlannedStmt *stmt, PgCircuitQueryInfo *qinfo)
{
	Oid			relid;
	double		rows;

	init_qinfo(qinfo);

	if (query == NULL)
		return;

	if (query->commandType != CMD_DELETE &&
		query->commandType != CMD_UPDATE)
		return;

	qinfo->is_write = true;
	relid = result_relation_oid(query);
	qinfo->relation_oid = relid;
	pg_circuit_relation_name(relid, qinfo->relation_name, sizeof(qinfo->relation_name));
	pg_circuit_relation_qualname(relid, qinfo->relation_qualname,
								 sizeof(qinfo->relation_qualname));
	qinfo->relation_size = pg_circuit_relation_size(relid);
	qinfo->has_where = query_has_where(query);
	rows = plan_estimated_rows(stmt);

	if (!qinfo->has_where && OidIsValid(relid))
	{
		double		tuples = pg_circuit_relation_estimated_tuples(relid);

		if (tuples > 0)
			rows = tuples;
	}

	qinfo->estimated_rows = clamp_estimated_rows(relid, rows);

	if (query->commandType == CMD_DELETE)
	{
		if (!qinfo->has_where)
			qinfo->kind = PGC_QKIND_DELETE_NO_WHERE;
		/* WHERE-qualified DELETE stays OTHER in Community */
		return;
	}

	if (!qinfo->has_where)
		qinfo->kind = PGC_QKIND_UPDATE_NO_WHERE;
	/* WHERE-qualified UPDATE stays OTHER in Community */
}

void
pg_circuit_analyze_plannedstmt(PlannedStmt *stmt, PgCircuitQueryInfo *qinfo)
{
	Oid			relid = InvalidOid;
	double		rows;
	bool		has_where = true;

	init_qinfo(qinfo);
	if (stmt == NULL)
		return;
	if (stmt->commandType != CMD_DELETE && stmt->commandType != CMD_UPDATE)
		return;

	qinfo->is_write = true;
	if (stmt->resultRelations != NIL)
	{
		Index		rti = (Index) linitial_int(stmt->resultRelations);
		RangeTblEntry *rte;

		if (rti > 0 && rti <= (Index) list_length(stmt->rtable))
		{
			rte = rt_fetch(rti, stmt->rtable);
			if (rte && rte->rtekind == RTE_RELATION)
				relid = rte->relid;
		}
	}
	qinfo->relation_oid = relid;
	pg_circuit_relation_name(relid, qinfo->relation_name, sizeof(qinfo->relation_name));
	pg_circuit_relation_qualname(relid, qinfo->relation_qualname,
								 sizeof(qinfo->relation_qualname));
	qinfo->relation_size = pg_circuit_relation_size(relid);
	rows = plan_estimated_rows(stmt);

	if (stmt->planTree && IsA(stmt->planTree, ModifyTable))
	{
		ModifyTable *mt = (ModifyTable *) stmt->planTree;
		Plan	   *sub = mt->plan.lefttree;

		if (sub != NULL && IsA(sub, SeqScan) && sub->qual == NULL)
			has_where = false;
	}
	qinfo->has_where = has_where;

	if (!has_where && OidIsValid(relid))
	{
		double		tuples = pg_circuit_relation_estimated_tuples(relid);

		if (tuples > 0)
			rows = tuples;
	}
	qinfo->estimated_rows = clamp_estimated_rows(relid, rows);

	if (stmt->commandType == CMD_DELETE)
	{
		if (!has_where)
			qinfo->kind = PGC_QKIND_DELETE_NO_WHERE;
		return;
	}
	if (!has_where)
		qinfo->kind = PGC_QKIND_UPDATE_NO_WHERE;
}
