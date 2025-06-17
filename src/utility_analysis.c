/*-------------------------------------------------------------------------
 *
 * utility_analysis.c
 *		ProcessUtility classification for destructive + DDL ops
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "catalog/namespace.h"
#include "commands/defrem.h"
#include "miscadmin.h"
#include "nodes/parsenodes.h"
#include "postmaster/autovacuum.h"
#include "tcop/utility.h"
#include "utils/lsyscache.h"

#include "pg_circuit.h"
#include "relations.h"
#include "risk_engine.h"

#if PG_VERSION_NUM < 170000
/* PG16 uses Is*; PG17+ renamed to Am* macros in autovacuum.h */
#ifndef AmAutoVacuumWorkerProcess
#define AmAutoVacuumWorkerProcess() IsAutoVacuumWorkerProcess()
#endif
#endif

void		pg_circuit_analyze_utility(Node *parsetree, PgCircuitQueryInfo *qinfo);

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
	qinfo->concurrent = false;
	qinfo->rewrite_likely = false;
	qinfo->lock_class = PGC_LOCK_NONE;
}

static void
fill_relation_from_rangevar(RangeVar *rv, PgCircuitQueryInfo *qinfo)
{
	Oid			relid;

	if (rv == NULL)
		return;

	strlcpy(qinfo->relation_name, rv->relname, sizeof(qinfo->relation_name));
	relid = RangeVarGetRelid(rv, AccessShareLock, true);
	qinfo->relation_oid = relid;
	if (OidIsValid(relid))
	{
		pg_circuit_relation_qualname(relid, qinfo->relation_qualname,
									 sizeof(qinfo->relation_qualname));
		qinfo->relation_size = pg_circuit_relation_size(relid);
		qinfo->estimated_rows = pg_circuit_relation_estimated_tuples(relid);
	}
}

static bool
def_list_has_bool(List *options, const char *name)
{
	ListCell   *lc;

	foreach(lc, options)
	{
		DefElem    *opt = lfirst_node(DefElem, lc);

		if (strcmp(opt->defname, name) == 0)
			return defGetBoolean(opt);
	}
	return false;
}

/*
 * Classify ALTER TABLE subcommands conservatively.
 *
 * Lock requirements vary by PostgreSQL version and exact form (e.g. ADD COLUMN
 * with/without default). We mark HIGH when ACCESS EXCLUSIVE or table rewrite
 * is commonly required, and document uncertainty in ddl_detail.
 */
static void
classify_alter_table(AlterTableStmt *stmt, PgCircuitQueryInfo *qinfo)
{
	ListCell   *lc;
	PgCircuitLockClass worst = PGC_LOCK_LOW;
	bool		rewrite = false;
	const char *detail = "ALTER TABLE";

	qinfo->is_write = true;
	qinfo->kind = PGC_QKIND_ALTER_TABLE;
	fill_relation_from_rangevar(stmt->relation, qinfo);

	foreach(lc, stmt->cmds)
	{
		AlterTableCmd *cmd = lfirst_node(AlterTableCmd, lc);

		switch (cmd->subtype)
		{
			case AT_AddColumn:
				/* Often weaker locks for plain nullable ADD COLUMN; still MEDIUM */
				if (worst < PGC_LOCK_MEDIUM)
					worst = PGC_LOCK_MEDIUM;
				detail = "ADD COLUMN";
				break;
			case AT_DropColumn:
				worst = PGC_LOCK_HIGH;
				rewrite = true;
				detail = "DROP COLUMN";
				break;
			case AT_AlterColumnType:
				worst = PGC_LOCK_HIGH;
				rewrite = true;
				detail = "ALTER COLUMN TYPE";
				break;
			case AT_SetNotNull:
				worst = PGC_LOCK_HIGH;
				detail = "SET NOT NULL";
				break;
			case AT_DropNotNull:
				if (worst < PGC_LOCK_MEDIUM)
					worst = PGC_LOCK_MEDIUM;
				detail = "DROP NOT NULL";
				break;
			case AT_AddConstraint:
				worst = PGC_LOCK_HIGH;
				detail = "ADD CONSTRAINT";
				break;
			case AT_DropConstraint:
				worst = PGC_LOCK_HIGH;
				detail = "DROP CONSTRAINT";
				break;
			case AT_SetTableSpace:
			case AT_SetLogged:
			case AT_SetUnLogged:
				worst = PGC_LOCK_HIGH;
				rewrite = true;
				detail = "storage rewrite ALTER";
				break;
			default:
				/* Unknown / low-impact forms stay at least LOW */
				if (worst < PGC_LOCK_LOW)
					worst = PGC_LOCK_LOW;
				detail = "ALTER TABLE (other)";
				break;
		}
	}

	qinfo->lock_class = worst;
	qinfo->rewrite_likely = rewrite;
	strlcpy(qinfo->ddl_detail, detail, sizeof(qinfo->ddl_detail));
}

void
pg_circuit_analyze_utility(Node *parsetree, PgCircuitQueryInfo *qinfo)
{
	init_qinfo(qinfo);

	if (parsetree == NULL)
		return;

	/* Never classify autovacuum worker maintenance as user DDL */
	if (AmAutoVacuumWorkerProcess())
		return;

	switch (nodeTag(parsetree))
	{
		case T_TruncateStmt:
			{
				TruncateStmt *stmt = (TruncateStmt *) parsetree;
				RangeVar   *rv = NULL;

				qinfo->is_write = true;
				qinfo->kind = PGC_QKIND_TRUNCATE;
				qinfo->lock_class = PGC_LOCK_HIGH;
				strlcpy(qinfo->ddl_detail, "TRUNCATE", sizeof(qinfo->ddl_detail));
				if (stmt->relations != NIL)
					rv = linitial_node(RangeVar, stmt->relations);
				fill_relation_from_rangevar(rv, qinfo);
				break;
			}
		case T_DropStmt:
			{
				DropStmt   *stmt = (DropStmt *) parsetree;

				if (stmt->removeType == OBJECT_TABLE)
				{
					List	   *objname;
					RangeVar   *rv;

					qinfo->is_write = true;
					qinfo->kind = PGC_QKIND_DROP_TABLE;
					qinfo->lock_class = PGC_LOCK_HIGH;
					strlcpy(qinfo->ddl_detail, "DROP TABLE", sizeof(qinfo->ddl_detail));
					if (stmt->objects != NIL)
					{
						objname = linitial(stmt->objects);
						rv = makeRangeVarFromNameList(objname);
						fill_relation_from_rangevar(rv, qinfo);
						if (stmt->missing_ok && !OidIsValid(qinfo->relation_oid))
						{
							init_qinfo(qinfo);
							return;
						}
					}
				}
				break;
			}
		case T_DropdbStmt:
			{
				DropdbStmt *stmt = (DropdbStmt *) parsetree;

				qinfo->is_write = true;
				qinfo->kind = PGC_QKIND_DROP_DATABASE;
				qinfo->lock_class = PGC_LOCK_HIGH;
				strlcpy(qinfo->ddl_detail, "DROP DATABASE", sizeof(qinfo->ddl_detail));
				if (stmt->dbname != NULL)
					strlcpy(qinfo->relation_name, stmt->dbname,
							sizeof(qinfo->relation_name));
				break;
			}
		case T_AlterTableStmt:
			classify_alter_table((AlterTableStmt *) parsetree, qinfo);
			break;
		case T_RenameStmt:
			{
				RenameStmt *stmt = (RenameStmt *) parsetree;

				if (stmt->renameType == OBJECT_TABLE ||
					stmt->renameType == OBJECT_COLUMN)
				{
					qinfo->is_write = true;
					qinfo->kind = PGC_QKIND_ALTER_TABLE;
					qinfo->lock_class = PGC_LOCK_HIGH;
					strlcpy(qinfo->ddl_detail,
							stmt->renameType == OBJECT_COLUMN ?
							"RENAME COLUMN" : "RENAME TABLE",
							sizeof(qinfo->ddl_detail));
					fill_relation_from_rangevar(stmt->relation, qinfo);
				}
				break;
			}
		case T_IndexStmt:
			{
				IndexStmt  *stmt = (IndexStmt *) parsetree;

				qinfo->is_write = true;
				qinfo->concurrent = stmt->concurrent;
				fill_relation_from_rangevar(stmt->relation, qinfo);
				if (stmt->concurrent)
				{
					qinfo->kind = PGC_QKIND_CREATE_INDEX_CONCURRENTLY;
					qinfo->lock_class = PGC_LOCK_LOW;
					strlcpy(qinfo->ddl_detail, "CREATE INDEX CONCURRENTLY",
							sizeof(qinfo->ddl_detail));
				}
				else
				{
					qinfo->kind = PGC_QKIND_CREATE_INDEX;
					qinfo->lock_class = PGC_LOCK_MEDIUM;
					strlcpy(qinfo->ddl_detail, "CREATE INDEX",
							sizeof(qinfo->ddl_detail));
				}
				break;
			}
		case T_ReindexStmt:
			{
				ReindexStmt *stmt = (ReindexStmt *) parsetree;
				bool		concurrent;

				concurrent = def_list_has_bool(stmt->params, "concurrently");
				qinfo->is_write = true;
				qinfo->concurrent = concurrent;
				qinfo->kind = PGC_QKIND_REINDEX;
				qinfo->lock_class = concurrent ? PGC_LOCK_LOW : PGC_LOCK_HIGH;
				strlcpy(qinfo->ddl_detail,
						concurrent ? "REINDEX CONCURRENTLY" : "REINDEX",
						sizeof(qinfo->ddl_detail));
				fill_relation_from_rangevar(stmt->relation, qinfo);
				break;
			}
		case T_VacuumStmt:
			{
				VacuumStmt *stmt = (VacuumStmt *) parsetree;
				bool		is_full;

				if (!stmt->is_vacuumcmd)
					break;		/* ANALYZE — leave unaffected */

				is_full = def_list_has_bool(stmt->options, "full");
				if (!is_full)
					break;		/* ordinary VACUUM — leave unaffected */

				qinfo->is_write = true;
				qinfo->kind = PGC_QKIND_VACUUM_FULL;
				qinfo->lock_class = PGC_LOCK_HIGH;
				qinfo->rewrite_likely = true;
				strlcpy(qinfo->ddl_detail, "VACUUM FULL", sizeof(qinfo->ddl_detail));
				if (stmt->rels != NIL)
				{
					VacuumRelation *vrel = linitial_node(VacuumRelation, stmt->rels);

					fill_relation_from_rangevar(vrel->relation, qinfo);
				}
				break;
			}
		case T_ClusterStmt:
			{
				ClusterStmt *stmt = (ClusterStmt *) parsetree;

				qinfo->is_write = true;
				qinfo->kind = PGC_QKIND_CLUSTER;
				qinfo->lock_class = PGC_LOCK_HIGH;
				qinfo->rewrite_likely = true;
				strlcpy(qinfo->ddl_detail, "CLUSTER", sizeof(qinfo->ddl_detail));
				fill_relation_from_rangevar(stmt->relation, qinfo);
				break;
			}
		default:
			break;
	}
}
