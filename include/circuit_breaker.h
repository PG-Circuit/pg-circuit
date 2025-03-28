/*-------------------------------------------------------------------------
 *
 * circuit_breaker.h
 *		Decision application for PG Circuit
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_CIRCUIT_CIRCUIT_BREAKER_H
#define PG_CIRCUIT_CIRCUIT_BREAKER_H

#include "pg_circuit.h"
#include "policy.h"
#include "risk_engine.h"
#include "runtime_state.h"

extern PgCircuitDecision pg_circuit_decide(const PgCircuitRiskAssessment *assessment);
extern void pg_circuit_apply_decision(const PgCircuitRiskAssessment *assessment,
									  const PgCircuitRuntimeState *runtime,
									  const PgCircuitQueryInfo *qinfo);
extern void pg_circuit_apply_decision_ex(const PgCircuitRiskAssessment *assessment,
										 const PgCircuitRuntimeState *runtime,
										 const PgCircuitQueryInfo *qinfo,
										 const PgCircuitPolicyResult *policy);

#endif							/* PG_CIRCUIT_CIRCUIT_BREAKER_H */
