/*-------------------------------------------------------------------------
 *
 * policy_stub.c
 *		Community stub — no local policy engine
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "utils/timestamp.h"

#include "policy.h"

void
pg_circuit_collect_identity(const PgCircuitQueryInfo *qinfo,
							PgCircuitIdentity *ident)
{
	(void) qinfo;
	MemSet(ident, 0, sizeof(*ident));
}

void
pg_circuit_policy_evaluate(const PgCircuitQueryInfo *qinfo,
						   const PgCircuitRuntimeState *runtime,
						   PgCircuitRiskAssessment *assessment,
						   PgCircuitPolicyResult *result)
{
	(void) qinfo;
	(void) runtime;

	MemSet(result, 0, sizeof(*result));
	result->matched = false;
	result->action = PGC_PACTION_NONE;
	result->base_decision = assessment->decision;
	result->final_decision = assessment->decision;
}

int
pg_circuit_reload_policies_internal(void)
{
	return 0;
}

const char *
pg_circuit_policy_action_name(PgCircuitPolicyAction action)
{
	switch (action)
	{
		case PGC_PACTION_ALLOW:
			return "allow";
		case PGC_PACTION_WARN:
			return "warn";
		case PGC_PACTION_BLOCK:
			return "block";
		case PGC_PACTION_AUDIT:
			return "audit";
		case PGC_PACTION_NONE:
		default:
			return "none";
	}
}

bool
pg_circuit_parse_policy_action(const char *name, PgCircuitPolicyAction *action)
{
	if (name == NULL || action == NULL)
		return false;
	*action = PGC_PACTION_NONE;
	return (pg_strcasecmp(name, "none") == 0);
}

bool
pg_circuit_maintenance_window_active(int dow, int start_min, int end_min,
									 TimestampTz now)
{
	(void) dow;
	(void) start_min;
	(void) end_min;
	(void) now;
	return false;
}
