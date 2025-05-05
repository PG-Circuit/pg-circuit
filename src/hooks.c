/*-------------------------------------------------------------------------
 *
 * hooks.c
 *		Hook registration with correct chaining (Community)
 *
 * Fast path:
 *   cheap classify -> OTHER/safe -> return (no runtime collection)
 *   potentially risky -> collect runtime -> score -> decide
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/parallel.h"
#include "commands/defrem.h"
#include "executor/executor.h"
#include "miscadmin.h"
#include "optimizer/planner.h"
#include "tcop/utility.h"
#include "utils/elog.h"

#include "circuit_breaker.h"
#include "event.h"
#include "fault.h"
#include "hooks.h"
#include "pg_circuit.h"
#include "policy.h"
#include "risk_engine.h"
#include "runtime_state.h"

extern void pg_circuit_analyze_query(Query *query, PlannedStmt *stmt,
									 PgCircuitQueryInfo *qinfo);
extern void pg_circuit_analyze_plannedstmt(PlannedStmt *stmt,
										   PgCircuitQueryInfo *qinfo);
extern void pg_circuit_analyze_utility(Node *parsetree,
									   PgCircuitQueryInfo *qinfo);

static planner_hook_type prev_planner_hook = NULL;
static ProcessUtility_hook_type prev_ProcessUtility_hook = NULL;
static ExecutorStart_hook_type prev_ExecutorStart_hook = NULL;
static ExecutorEnd_hook_type prev_ExecutorEnd_hook = NULL;

static bool
should_skip_check(void)
{
	if (!pg_circuit_enabled)
		return true;
	if (IsParallelWorker())
		return true;
	return false;
}

static void
debug_log_decision(const PgCircuitQueryInfo *qinfo,
				   const PgCircuitRuntimeState *runtime,
				   const PgCircuitRiskAssessment *assessment)
{
	char	   *findings;

	if (!pg_circuit_debug)
		return;

	findings = pg_circuit_format_findings(assessment);
	elog(DEBUG1,
		 "pg_circuit decision=%s score=%d mode=%s pressure=%d kind=%d rel=%s rows=%.0f size=%lld\n%s",
		 pg_circuit_decision_name(assessment->decision),
		 assessment->score,
		 pg_circuit_safety_mode_name(runtime->mode),
		 runtime->pressure_score,
		 (int) qinfo->kind,
		 qinfo->relation_qualname[0] ? qinfo->relation_qualname : qinfo->relation_name,
		 qinfo->estimated_rows,
		 (long long) qinfo->relation_size,
		 findings);
	pfree(findings);
}

static void
handle_internal_failure(ErrorData *edata)
{
	if (pg_circuit_mode == PGC_MODE_ENFORCE && pg_circuit_fail_closed)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("PG Circuit internal error (fail_closed): %s",
						edata->message),
				 errhint("set pg_circuit.fail_closed=off to fail open, or fix the underlying fault")));
	}

	ereport(WARNING,
			(errcode(ERRCODE_WARNING),
			 errmsg("PG Circuit internal error (fail_open): %s", edata->message),
			 errhint("statement allowed; check logs and extension health")));
}

static void
evaluate_and_apply(PgCircuitQueryInfo *qinfo)
{
	PgCircuitRuntimeState runtime;
	PgCircuitRiskAssessment assessment;
	PgCircuitRiskEvent event;
	PgCircuitPolicyResult policy;
	MemoryContext caller_ctx = CurrentMemoryContext;

	/* Fast path: non-risky operations skip runtime collection */
	if (qinfo->kind == PGC_QKIND_OTHER)
		return;

	PG_TRY();
	{
		if (pg_circuit_fault_is("runtime"))
			elog(ERROR, "injected runtime-state failure");

		pg_circuit_collect_runtime_state(&runtime);
		pg_circuit_assess_query(qinfo, &runtime, &assessment);
		pg_circuit_policy_evaluate(qinfo, &runtime, &assessment, &policy);
		pg_circuit_fill_risk_event(&event, qinfo, &runtime, &assessment, &policy);
		pg_circuit_persist_risk_event(&event, qinfo, &assessment, &policy);
		debug_log_decision(qinfo, &runtime, &assessment);
		pg_circuit_apply_decision_ex(&assessment, &runtime, qinfo, &policy);
		pg_circuit_free_assessment(&assessment);
	}
	PG_CATCH();
	{
		ErrorData  *edata;

		MemoryContextSwitchTo(caller_ctx);
		edata = CopyErrorData();
		FlushErrorState();

		if (edata->sqlerrcode == ERRCODE_INSUFFICIENT_PRIVILEGE)
		{
			ReThrowError(edata);
		}

		handle_internal_failure(edata);
		FreeErrorData(edata);
	}
	PG_END_TRY();
}

static PlannedStmt *
pg_circuit_planner(Query *parse,
				   const char *query_string,
				   int cursorOptions,
				   ParamListInfo boundParams)
{
	if (prev_planner_hook)
		return prev_planner_hook(parse, query_string, cursorOptions, boundParams);
	return standard_planner(parse, query_string, cursorOptions, boundParams);
}

static bool
options_has_full(List *options)
{
	ListCell   *lc;

	foreach(lc, options)
	{
		DefElem    *opt = lfirst_node(DefElem, lc);

		if (strcmp(opt->defname, "full") == 0)
			return true;
	}
	return false;
}

static bool
utility_may_be_risky(Node *parsetree)
{
	if (parsetree == NULL)
		return false;

	switch (nodeTag(parsetree))
	{
		case T_TruncateStmt:
		case T_DropdbStmt:
		case T_AlterTableStmt:
		case T_IndexStmt:
		case T_ReindexStmt:
		case T_ClusterStmt:
			return true;
		case T_RenameStmt:
			{
				RenameStmt *stmt = (RenameStmt *) parsetree;

				return (stmt->renameType == OBJECT_TABLE ||
						stmt->renameType == OBJECT_COLUMN);
			}
		case T_DropStmt:
			return (((DropStmt *) parsetree)->removeType == OBJECT_TABLE);
		case T_VacuumStmt:
			{
				VacuumStmt *stmt = (VacuumStmt *) parsetree;

				if (!stmt->is_vacuumcmd)
					return false;
				return options_has_full(stmt->options);
			}
		default:
			return false;
	}
}

static void
pg_circuit_process_utility(PlannedStmt *pstmt,
						   const char *queryString,
						   bool readOnlyTree,
						   ProcessUtilityContext context,
						   ParamListInfo params,
						   QueryEnvironment *queryEnv,
						   DestReceiver *dest,
						   QueryCompletion *qc)
{
	Node	   *parsetree = pstmt->utilityStmt;
	PgCircuitQueryInfo qinfo;

	if (!should_skip_check() && utility_may_be_risky(parsetree))
	{
		pg_circuit_analyze_utility(parsetree, &qinfo);
		evaluate_and_apply(&qinfo);
	}

	if (prev_ProcessUtility_hook)
		prev_ProcessUtility_hook(pstmt, queryString, readOnlyTree,
								 context, params, queryEnv, dest, qc);
	else
		standard_ProcessUtility(pstmt, queryString, readOnlyTree,
								context, params, queryEnv, dest, qc);
}

static void
pg_circuit_executor_start(QueryDesc *queryDesc, int eflags)
{
	PgCircuitQueryInfo qinfo;

	if (!should_skip_check() &&
		queryDesc != NULL &&
		queryDesc->plannedstmt != NULL &&
		(queryDesc->plannedstmt->commandType == CMD_DELETE ||
		 queryDesc->plannedstmt->commandType == CMD_UPDATE) &&
		(eflags & EXEC_FLAG_EXPLAIN_ONLY) == 0)
	{
		pg_circuit_analyze_plannedstmt(queryDesc->plannedstmt, &qinfo);
		evaluate_and_apply(&qinfo);
	}

	if (prev_ExecutorStart_hook)
		prev_ExecutorStart_hook(queryDesc, eflags);
	else
		standard_ExecutorStart(queryDesc, eflags);
}

static void
pg_circuit_executor_end(QueryDesc *queryDesc)
{
	if (prev_ExecutorEnd_hook)
		prev_ExecutorEnd_hook(queryDesc);
	else
		standard_ExecutorEnd(queryDesc);
}

void
pg_circuit_install_hooks(void)
{
	prev_planner_hook = planner_hook;
	planner_hook = pg_circuit_planner;

	prev_ProcessUtility_hook = ProcessUtility_hook;
	ProcessUtility_hook = pg_circuit_process_utility;

	prev_ExecutorStart_hook = ExecutorStart_hook;
	ExecutorStart_hook = pg_circuit_executor_start;

	prev_ExecutorEnd_hook = ExecutorEnd_hook;
	ExecutorEnd_hook = pg_circuit_executor_end;
}

void
pg_circuit_uninstall_hooks(void)
{
	if (planner_hook == pg_circuit_planner)
		planner_hook = prev_planner_hook;

	if (ProcessUtility_hook == pg_circuit_process_utility)
		ProcessUtility_hook = prev_ProcessUtility_hook;

	if (ExecutorStart_hook == pg_circuit_executor_start)
		ExecutorStart_hook = prev_ExecutorStart_hook;

	if (ExecutorEnd_hook == pg_circuit_executor_end)
		ExecutorEnd_hook = prev_ExecutorEnd_hook;
}
