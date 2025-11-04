/*-------------------------------------------------------------------------
 *
 * pg_circuit.c
 *		Extension entry point and SQL API (Community)
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "commands/dbcommands.h"
#include "fmgr.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/timestamp.h"

#include "blocker_graph.h"
#include "event.h"
#include "hooks.h"
#include "pg_circuit.h"
#include "risk_engine.h"
#include "runtime_state.h"
#include "shmem.h"

PG_MODULE_MAGIC;

void		_PG_init(void);
void		_PG_fini(void);

void
_PG_init(void)
{
	pg_circuit_register_gucs();
	pg_circuit_shmem_register();
	pg_circuit_install_hooks();
}

void
_PG_fini(void)
{
	pg_circuit_uninstall_hooks();
}

PG_FUNCTION_INFO_V1(pg_circuit_version);

Datum
pg_circuit_version(PG_FUNCTION_ARGS)
{
	PG_RETURN_TEXT_P(cstring_to_text(PG_CIRCUIT_VERSION));
}

PG_FUNCTION_INFO_V1(pg_circuit_status);

Datum
pg_circuit_status(PG_FUNCTION_ARGS)
{
	TupleDesc	tupdesc;
	Datum		values[8];
	bool		nulls[8];
	HeapTuple	tuple;
	PgCircuitRuntimeState runtime;
	char		pgver[32];

	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		elog(ERROR, "return type must be a row type");

	pg_circuit_collect_runtime_state(&runtime);
	snprintf(pgver, sizeof(pgver), "%d", PG_VERSION_NUM);

	MemSet(nulls, 0, sizeof(nulls));
	values[0] = CStringGetTextDatum(PG_CIRCUIT_VERSION);
	values[1] = CStringGetTextDatum(pgver);
	values[2] = BoolGetDatum(pg_circuit_enabled);
	values[3] = CStringGetTextDatum(pg_circuit_mode_name(pg_circuit_mode));
	values[4] = CStringGetTextDatum(pg_circuit_runtime_cfg_name(pg_circuit_runtime_mode));
	values[5] = CStringGetTextDatum(pg_circuit_safety_mode_name(runtime.mode));
	values[6] = Int32GetDatum(pg_circuit_risk_warn_threshold);
	values[7] = Int32GetDatum(pg_circuit_risk_block_threshold);

	tuple = heap_form_tuple(tupdesc, values, nulls);
	PG_RETURN_DATUM(HeapTupleGetDatum(tuple));
}

PG_FUNCTION_INFO_V1(pg_circuit_runtime_state);

Datum
pg_circuit_runtime_state(PG_FUNCTION_ARGS)
{
	TupleDesc	tupdesc;
	Datum		values[22];
	bool		nulls[22];
	HeapTuple	tuple;
	PgCircuitRuntimeState runtime;

	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		elog(ERROR, "return type must be a row type");

	pg_circuit_collect_runtime_state(&runtime);

	MemSet(nulls, 0, sizeof(nulls));
	values[0] = CStringGetTextDatum(pg_circuit_safety_mode_name(runtime.mode));
	values[1] = Int32GetDatum(runtime.pressure_score);
	values[2] = Int32GetDatum(runtime.active_transactions);
	values[3] = Int32GetDatum(runtime.long_transactions);
	values[4] = Int32GetDatum(runtime.idle_in_transaction);
	values[5] = Int32GetDatum(runtime.blocked_sessions);
	values[6] = Int32GetDatum(runtime.blocking_sessions);
	values[7] = Float8GetDatum(runtime.max_replication_lag_seconds);
	values[8] = CStringGetTextDatum(pg_circuit_wal_pressure_name(runtime.wal_pressure_level));
	values[9] = Float8GetDatum(runtime.wal_bytes_per_sec);
	values[10] = Int32GetDatum(runtime.active_connections);
	values[11] = Int32GetDatum(runtime.connection_pressure_score);
	values[12] = Float8GetDatum(runtime.oldest_transaction_age_seconds);
	values[13] = Int32GetDatum(runtime.max_lock_chain_depth);
	values[14] = Int32GetDatum(runtime.max_lock_descendants);
	values[15] = Int32GetDatum(runtime.replica_count);
	values[16] = CStringGetTextDatum(pg_circuit_replication_health_name(runtime.replication_health));
	values[17] = Float8GetDatum(runtime.max_write_lag_seconds);
	values[18] = Float8GetDatum(runtime.max_flush_lag_seconds);
	values[19] = Float8GetDatum(runtime.max_replay_lag_seconds);
	values[20] = Int32GetDatum(runtime.pressure_tx);
	{
		char	   *contrib = pg_circuit_format_pressure_contributors(&runtime);

		values[21] = CStringGetTextDatum(contrib);
		pfree(contrib);
	}

	if (runtime.max_write_lag_seconds < 0)
		nulls[17] = true;
	if (runtime.max_flush_lag_seconds < 0)
		nulls[18] = true;
	if (runtime.max_replay_lag_seconds < 0)
		nulls[19] = true;

	tuple = heap_form_tuple(tupdesc, values, nulls);
	PG_RETURN_DATUM(HeapTupleGetDatum(tuple));
}

/*
 * Deterministic risk explanation from synthetic inputs — for tests and
 * developer inspection. Does not execute SQL and does not collect live state.
 */
PG_FUNCTION_INFO_V1(pg_circuit_explain_risk);

Datum
pg_circuit_explain_risk(PG_FUNCTION_ARGS)
{
	text	   *op_text = PG_GETARG_TEXT_PP(0);
	float8		estimated_rows = PG_GETARG_FLOAT8(1);
	int64		relation_size = PG_GETARG_INT64(2);
	text	   *mode_text = PG_GETARG_TEXT_PP(3);
	float8		lag = PG_GETARG_FLOAT8(4);
	TupleDesc	tupdesc;
	Datum		values[4];
	bool		nulls[4];
	HeapTuple	tuple;
	PgCircuitQueryInfo qinfo;
	PgCircuitRuntimeState runtime;
	PgCircuitRiskAssessment assessment;
	PgCircuitQueryKind kind;
	PgCircuitSafetyMode mode;
	char	   *op;
	char	   *mode_name;
	char	   *findings;

	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		elog(ERROR, "return type must be a row type");

	op = text_to_cstring(op_text);
	mode_name = text_to_cstring(mode_text);

	if (!pg_circuit_parse_query_kind(op, &kind))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("unknown pg_circuit operation \"%s\"", op)));
	if (!pg_circuit_parse_safety_mode(mode_name, &mode))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("unknown pg_circuit runtime_mode \"%s\"", mode_name)));

	MemSet(&qinfo, 0, sizeof(qinfo));
	qinfo.kind = kind;
	qinfo.estimated_rows = estimated_rows;
	qinfo.relation_size = relation_size;
	qinfo.is_write = true;
	strlcpy(qinfo.relation_name, "synthetic", sizeof(qinfo.relation_name));

	switch (kind)
	{
		case PGC_QKIND_ALTER_TABLE:
			qinfo.lock_class = PGC_LOCK_HIGH;
			qinfo.rewrite_likely = true;
			strlcpy(qinfo.ddl_detail, "ALTER COLUMN TYPE", sizeof(qinfo.ddl_detail));
			break;
		case PGC_QKIND_CREATE_INDEX:
			qinfo.lock_class = PGC_LOCK_MEDIUM;
			break;
		case PGC_QKIND_CREATE_INDEX_CONCURRENTLY:
			qinfo.concurrent = true;
			qinfo.lock_class = PGC_LOCK_LOW;
			break;
		case PGC_QKIND_REINDEX:
			qinfo.lock_class = PGC_LOCK_HIGH;
			break;
		case PGC_QKIND_VACUUM_FULL:
		case PGC_QKIND_CLUSTER:
			qinfo.lock_class = PGC_LOCK_HIGH;
			qinfo.rewrite_likely = true;
			break;
		default:
			break;
	}

	MemSet(&runtime, 0, sizeof(runtime));
	runtime.mode = mode;
	runtime.max_replication_lag_seconds = lag;
	runtime.pressure_score = 0;

	pg_circuit_assess_query(&qinfo, &runtime, &assessment);
	findings = pg_circuit_format_findings(&assessment);

	MemSet(nulls, 0, sizeof(nulls));
	values[0] = Int32GetDatum(assessment.score);
	values[1] = CStringGetTextDatum(pg_circuit_decision_name(assessment.decision));
	values[2] = CStringGetTextDatum(pg_circuit_risk_level_name(assessment.level));
	values[3] = CStringGetTextDatum(findings);

	tuple = heap_form_tuple(tupdesc, values, nulls);

	pg_circuit_free_assessment(&assessment);
	pfree(findings);
	pfree(op);
	pfree(mode_name);

	PG_RETURN_DATUM(HeapTupleGetDatum(tuple));
}

PG_FUNCTION_INFO_V1(pg_circuit_blockers);

Datum
pg_circuit_blockers(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;

	if (SRF_IS_FIRSTCALL())
	{
		MemoryContext oldcontext;
		TupleDesc	tupdesc;
		PgCircuitBlockerGraph *graph;

		funcctx = SRF_FIRSTCALL_INIT();
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
			elog(ERROR, "return type must be a row type");
		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		graph = palloc0(sizeof(PgCircuitBlockerGraph));
		pg_circuit_build_blocker_graph(graph);
		funcctx->user_fctx = graph;
		funcctx->max_calls = list_length(graph->edges);

		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();

	{
		PgCircuitBlockerGraph *graph = (PgCircuitBlockerGraph *) funcctx->user_fctx;

		if (funcctx->call_cntr < (uint32) list_length(graph->edges))
		{
			PgCircuitBlockerEdge *edge;
			Datum		values[7];
			bool		nulls[7];
			HeapTuple	tuple;
			char		dbname[NAMEDATALEN];

			edge = (PgCircuitBlockerEdge *) list_nth(graph->edges,
													 (int) funcctx->call_cntr);
			MemSet(nulls, 0, sizeof(nulls));

			values[0] = Int32GetDatum(edge->blocked_pid);
			values[1] = Int32GetDatum(edge->blocking_pid);
			values[2] = Float8GetDatum(edge->blocked_for_seconds);
			values[3] = Float8GetDatum(edge->blocking_transaction_age_seconds);

			if (OidIsValid(edge->database_oid))
			{
				char	   *name = get_database_name(edge->database_oid);

				if (name)
				{
					strlcpy(dbname, name, sizeof(dbname));
					pfree(name);
					values[4] = CStringGetTextDatum(dbname);
				}
				else
					nulls[4] = true;
			}
			else
				nulls[4] = true;

			if (edge->relation_name[0])
				values[5] = CStringGetTextDatum(edge->relation_name);
			else
				nulls[5] = true;

			values[6] = CStringGetTextDatum(edge->lock_mode[0] ?
										   edge->lock_mode : edge->lock_type);

			tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
			SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
		}

		pg_circuit_blocker_graph_free(graph);
		SRF_RETURN_DONE(funcctx);
	}
}

PG_FUNCTION_INFO_V1(pg_circuit_lock_summary);

Datum
pg_circuit_lock_summary(PG_FUNCTION_ARGS)
{
	TupleDesc	tupdesc;
	Datum		values[6];
	bool		nulls[6];
	HeapTuple	tuple;
	PgCircuitBlockerGraph graph;

	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		elog(ERROR, "return type must be a row type");

	pg_circuit_build_blocker_graph(&graph);

	MemSet(nulls, 0, sizeof(nulls));
	values[0] = Int32GetDatum(graph.blocked_sessions);
	values[1] = Int32GetDatum(graph.blocking_sessions);
	values[2] = Float8GetDatum(graph.max_blocked_for_seconds);
	values[3] = BoolGetDatum(graph.has_cycle);
	values[4] = Int32GetDatum(graph.max_chain_depth);
	values[5] = Int32GetDatum(graph.max_descendant_count);

	tuple = heap_form_tuple(tupdesc, values, nulls);
	pg_circuit_blocker_graph_free(&graph);
	PG_RETURN_DATUM(HeapTupleGetDatum(tuple));
}

/*
 * Extended synthetic explain with lock-pressure inputs.
 */
PG_FUNCTION_INFO_V1(pg_circuit_explain_risk_ex);

Datum
pg_circuit_explain_risk_ex(PG_FUNCTION_ARGS)
{
	text	   *op_text = PG_GETARG_TEXT_PP(0);
	float8		estimated_rows = PG_GETARG_FLOAT8(1);
	int64		relation_size = PG_GETARG_INT64(2);
	text	   *mode_text = PG_GETARG_TEXT_PP(3);
	float8		lag = PG_GETARG_FLOAT8(4);
	int32		blocked = PG_GETARG_INT32(5);
	int32		blocking = PG_GETARG_INT32(6);
	float8		max_wait = PG_GETARG_FLOAT8(7);
	TupleDesc	tupdesc;
	Datum		values[4];
	bool		nulls[4];
	HeapTuple	tuple;
	PgCircuitQueryInfo qinfo;
	PgCircuitRuntimeState runtime;
	PgCircuitRiskAssessment assessment;
	PgCircuitQueryKind kind;
	PgCircuitSafetyMode mode;
	char	   *op;
	char	   *mode_name;
	char	   *findings;

	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		elog(ERROR, "return type must be a row type");

	op = text_to_cstring(op_text);
	mode_name = text_to_cstring(mode_text);

	if (!pg_circuit_parse_query_kind(op, &kind))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("unknown pg_circuit operation \"%s\"", op)));
	if (!pg_circuit_parse_safety_mode(mode_name, &mode))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("unknown pg_circuit runtime_mode \"%s\"", mode_name)));

	MemSet(&qinfo, 0, sizeof(qinfo));
	qinfo.kind = kind;
	qinfo.estimated_rows = estimated_rows;
	qinfo.relation_size = relation_size;
	qinfo.is_write = true;
	strlcpy(qinfo.relation_name, "synthetic", sizeof(qinfo.relation_name));
	if (kind == PGC_QKIND_ALTER_TABLE || kind == PGC_QKIND_VACUUM_FULL ||
		kind == PGC_QKIND_CLUSTER || kind == PGC_QKIND_REINDEX)
	{
		qinfo.lock_class = PGC_LOCK_HIGH;
		qinfo.rewrite_likely = (kind != PGC_QKIND_REINDEX);
		strlcpy(qinfo.ddl_detail, "synthetic ddl", sizeof(qinfo.ddl_detail));
	}
	else if (kind == PGC_QKIND_CREATE_INDEX)
		qinfo.lock_class = PGC_LOCK_MEDIUM;

	MemSet(&runtime, 0, sizeof(runtime));
	runtime.mode = mode;
	runtime.max_replication_lag_seconds = lag;
	runtime.blocked_sessions = blocked;
	runtime.blocking_sessions = blocking;
	runtime.max_lock_wait_seconds = max_wait;

	pg_circuit_assess_query(&qinfo, &runtime, &assessment);
	findings = pg_circuit_format_findings(&assessment);

	MemSet(nulls, 0, sizeof(nulls));
	values[0] = Int32GetDatum(assessment.score);
	values[1] = CStringGetTextDatum(pg_circuit_decision_name(assessment.decision));
	values[2] = CStringGetTextDatum(pg_circuit_risk_level_name(assessment.level));
	values[3] = CStringGetTextDatum(findings);

	tuple = heap_form_tuple(tupdesc, values, nulls);
	pg_circuit_free_assessment(&assessment);
	pfree(findings);
	pfree(op);
	pfree(mode_name);

	PG_RETURN_DATUM(HeapTupleGetDatum(tuple));
}

PG_FUNCTION_INFO_V1(pg_circuit_events);

Datum
pg_circuit_events(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;

	if (SRF_IS_FIRSTCALL())
	{
		MemoryContext oldcontext;
		TupleDesc	tupdesc;
		PgCircuitSharedEvent *snapshot;
		int			n;
		int			cap;

		funcctx = SRF_FIRSTCALL_INIT();
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
			elog(ERROR, "return type must be a row type");
		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		cap = pg_circuit_event_history_size;
		if (cap < 1)
			cap = 1;
		snapshot = (PgCircuitSharedEvent *) palloc0(sizeof(PgCircuitSharedEvent) * (Size) cap);
		n = pg_circuit_event_snapshot(snapshot, cap);
		funcctx->user_fctx = snapshot;
		funcctx->max_calls = n;

		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();

	if (funcctx->call_cntr < funcctx->max_calls)
	{
		PgCircuitSharedEvent *snapshot =
			(PgCircuitSharedEvent *) funcctx->user_fctx;
		PgCircuitSharedEvent *ev = &snapshot[funcctx->call_cntr];
		Datum		values[15];
		bool		nulls[15];
		HeapTuple	tuple;

		MemSet(nulls, 0, sizeof(nulls));
		values[0] = TimestampTzGetDatum(ev->timestamp);
		values[1] = Int32GetDatum(ev->backend_pid);
		if (ev->database_name[0])
			values[2] = CStringGetTextDatum(ev->database_name);
		else
			nulls[2] = true;
		if (ev->user_name[0])
			values[3] = CStringGetTextDatum(ev->user_name);
		else
			nulls[3] = true;
		values[4] = CStringGetTextDatum(
			pg_circuit_query_kind_name((PgCircuitQueryKind) ev->operation));
		if (ev->relation_name[0])
			values[5] = CStringGetTextDatum(ev->relation_name);
		else
			nulls[5] = true;
		values[6] = Int32GetDatum(ev->risk_score);
		values[7] = CStringGetTextDatum(
			pg_circuit_risk_level_name(pg_circuit_score_to_level(ev->risk_score)));
		values[8] = CStringGetTextDatum(
			pg_circuit_safety_mode_name((PgCircuitSafetyMode) ev->runtime_mode));
		values[9] = CStringGetTextDatum(
			pg_circuit_decision_name((PgCircuitDecision) ev->decision));
		if (ev->rule_ids[0])
			values[10] = CStringGetTextDatum(ev->rule_ids);
		else
			nulls[10] = true;
		values[11] = Int64GetDatum(ev->event_id);
		values[12] = Int64GetDatum(ev->decision_id);
		if (ev->fingerprint[0])
			values[13] = CStringGetTextDatum(ev->fingerprint);
		else
			nulls[13] = true;
		values[14] = CStringGetTextDatum(
			pg_circuit_decision_name((PgCircuitDecision) ev->base_decision));

		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
	}

	SRF_RETURN_DONE(funcctx);
}
