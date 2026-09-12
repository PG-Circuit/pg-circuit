/*-------------------------------------------------------------------------
 *
 * circuit_breaker.c
 *		Apply ALLOW / WARN / BLOCK decisions
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "lib/stringinfo.h"
#include "utils/elog.h"

#include "circuit_breaker.h"
#include "pg_circuit.h"
#include "policy.h"
#include "runtime_state.h"

/*
 * Format a non-negative integer with thousands separators (48,192,211).
 */
static void
append_grouped_u64(StringInfo buf, uint64 value)
{
	char		tmp[32];
	char		out[48];
	int			len;
	int			i;
	int			o;
	int			digits;

	len = snprintf(tmp, sizeof(tmp), UINT64_FORMAT, value);
	if (len <= 0 || len >= (int) sizeof(tmp))
	{
		appendStringInfoString(buf, tmp);
		return;
	}

	digits = 0;
	o = 0;
	for (i = len - 1; i >= 0; i--)
	{
		if (digits > 0 && (digits % 3) == 0)
			out[o++] = ',';
		out[o++] = tmp[i];
		digits++;
	}
	out[o] = '\0';

	/* reverse into buf */
	for (i = o - 1; i >= 0; i--)
		appendStringInfoChar(buf, out[i]);
}

/*
 * Compact operator-facing DETAIL template (shared by ERROR and WARNING):
 *
 *   Risk: 95/100
 *   Estimated rows: 3
 *   Relation: public.users
 *   Operating mode: enforce
 *   Runtime mode: NORMAL
 *   Pressure: 12/100
 *   Replication lag: 0.0s
 *   Decision: BLOCK
 *
 * Community: effective runtime mode stays NORMAL; pressure is display-only.
 */
static void
format_decision_detail(StringInfo detail,
					   const PgCircuitRiskAssessment *assessment,
					   const PgCircuitRuntimeState *runtime,
					   const PgCircuitQueryInfo *qinfo,
					   const PgCircuitPolicyResult *policy)
{
	PgCircuitDecision decision = assessment->decision;

	appendStringInfo(detail, "Risk: %d/100", assessment->score);

	if (qinfo->estimated_rows >= 0)
	{
		appendStringInfoString(detail, "\nEstimated rows: ");
		append_grouped_u64(detail, (uint64) qinfo->estimated_rows);
	}

	if (qinfo->relation_qualname[0])
		appendStringInfo(detail, "\nRelation: %s", qinfo->relation_qualname);
	else if (qinfo->relation_name[0] &&
			 strcmp(qinfo->relation_name, "?") != 0)
		appendStringInfo(detail, "\nRelation: %s", qinfo->relation_name);

	appendStringInfo(detail, "\nOperating mode: %s",
					 pg_circuit_mode_name(pg_circuit_mode));
	appendStringInfo(detail, "\nRuntime mode: %s",
					 pg_circuit_safety_mode_name(runtime->mode));
	appendStringInfo(detail, "\nPressure: %d/100", runtime->pressure_score);
	appendStringInfo(detail, "\nReplication lag: %.1fs",
					 runtime->max_replication_lag_seconds);

	if (runtime->long_transactions > 0)
		appendStringInfo(detail, "\nLong transactions: %d",
						 runtime->long_transactions);

	if (runtime->blocked_sessions > 0)
		appendStringInfo(detail, "\nBlocked sessions: %d",
						 runtime->blocked_sessions);

	(void) policy;

	appendStringInfo(detail, "\nDecision: %s",
					 pg_circuit_decision_name(decision));
}

PgCircuitDecision
pg_circuit_decide(const PgCircuitRiskAssessment *assessment)
{
	return assessment->decision;
}

void
pg_circuit_apply_decision(const PgCircuitRiskAssessment *assessment,
						  const PgCircuitRuntimeState *runtime,
						  const PgCircuitQueryInfo *qinfo)
{
	pg_circuit_apply_decision_ex(assessment, runtime, qinfo, NULL);
}

void
pg_circuit_apply_decision_ex(const PgCircuitRiskAssessment *assessment,
							 const PgCircuitRuntimeState *runtime,
							 const PgCircuitQueryInfo *qinfo,
							 const PgCircuitPolicyResult *policy)
{
	const char *rule;
	StringInfoData detail;

	if (assessment->decision == PGC_DECISION_ALLOW)
		return;

	rule = assessment->primary_rule[0] ? assessment->primary_rule : "PGC000";

	initStringInfo(&detail);
	format_decision_detail(&detail, assessment, runtime, qinfo, policy);

	if (assessment->decision == PGC_DECISION_BLOCK)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("PG Circuit [%s] blocked high-risk operation", rule),
				 errdetail("%s", detail.data),
				 errhint("retry in smaller batches (add WHERE); then SELECT * FROM pg_circuit_events() and pg_circuit_explain_risk(...); adjust pg_circuit.mode or risk_*_threshold")));
	}

	ereport(WARNING,
			(errcode(ERRCODE_WARNING),
			 errmsg("PG Circuit [%s] high-risk operation", rule),
			 errdetail("%s", detail.data),
			 errhint("set pg_circuit.mode=enforce to block, or reduce scope; inspect with pg_circuit_events() / pg_circuit_explain_risk(...)")));
}
