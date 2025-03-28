/*-------------------------------------------------------------------------
 *
 * event.h
 *		Structured risk events and bounded shared-memory history
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_CIRCUIT_EVENT_H
#define PG_CIRCUIT_EVENT_H

#include "pg_circuit.h"
#include "policy.h"
#include "risk_engine.h"
#include "runtime_state.h"
#include "utils/timestamp.h"

typedef struct PgCircuitRiskEvent
{
	TimestampTz timestamp;
	int			backend_pid;
	Oid			database_oid;
	Oid			user_oid;
	char		primary_rule[16];
	int			risk_score;
	PgCircuitDecision decision;
	PgCircuitDecision base_decision;
	PgCircuitSafetyMode runtime_mode;
	Oid			relation_oid;
	PgCircuitQueryKind operation;
} PgCircuitRiskEvent;

extern void pg_circuit_fill_risk_event(PgCircuitRiskEvent *event,
									   const PgCircuitQueryInfo *qinfo,
									   const PgCircuitRuntimeState *runtime,
									   const PgCircuitRiskAssessment *assessment,
									   const PgCircuitPolicyResult *policy);
extern void pg_circuit_persist_risk_event(const PgCircuitRiskEvent *event,
										  const PgCircuitQueryInfo *qinfo,
										  const PgCircuitRiskAssessment *assessment,
										  const PgCircuitPolicyResult *policy);
extern const char *pg_circuit_query_kind_name(PgCircuitQueryKind kind);

#endif							/* PG_CIRCUIT_EVENT_H */
