/*-------------------------------------------------------------------------
 *
 * policy.h
 *		Community policy stub (full engine is Pro-only)
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_CIRCUIT_POLICY_H
#define PG_CIRCUIT_POLICY_H

#include "pg_circuit.h"
#include "risk_engine.h"
#include "runtime_state.h"

typedef enum PgCircuitPolicyAction
{
	PGC_PACTION_NONE = 0,
	PGC_PACTION_ALLOW,
	PGC_PACTION_WARN,
	PGC_PACTION_BLOCK,
	PGC_PACTION_AUDIT
} PgCircuitPolicyAction;

typedef struct PgCircuitIdentity
{
	char		database_name[NAMEDATALEN];
	char		current_user[NAMEDATALEN];
	char		session_user[NAMEDATALEN];
	char		application_name[NAMEDATALEN];
	char		schema_name[NAMEDATALEN];
	char		relation_name[NAMEDATALEN];
} PgCircuitIdentity;

typedef struct PgCircuitPolicyResult
{
	bool		matched;
	bool		exception_applied;
	bool		audit_only;
	bool		outside_maintenance_window;
	int64		policy_id;
	int64		policy_rule_id;
	int64		exception_id;
	int			priority;
	PgCircuitDecision base_decision;
	PgCircuitDecision final_decision;
	PgCircuitPolicyAction action;
	char		policy_name[NAMEDATALEN];
	char		matched_rule[32];
	char		reason[256];
} PgCircuitPolicyResult;

extern void pg_circuit_collect_identity(const PgCircuitQueryInfo *qinfo,
										PgCircuitIdentity *ident);
extern void pg_circuit_policy_evaluate(const PgCircuitQueryInfo *qinfo,
									   const PgCircuitRuntimeState *runtime,
									   PgCircuitRiskAssessment *assessment,
									   PgCircuitPolicyResult *result);
extern int	pg_circuit_reload_policies_internal(void);
extern const char *pg_circuit_policy_action_name(PgCircuitPolicyAction action);
extern bool pg_circuit_parse_policy_action(const char *name,
										   PgCircuitPolicyAction *action);
extern bool pg_circuit_maintenance_window_active(int dow,
												 int start_min,
												 int end_min,
												 TimestampTz now);

#endif							/* PG_CIRCUIT_POLICY_H */
