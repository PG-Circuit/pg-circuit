/*-------------------------------------------------------------------------
 *
 * reporting.c
 *		Shared formatting helpers
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "pg_circuit.h"

PgCircuitRiskLevel
pg_circuit_score_to_level(int score)
{
	if (score >= 80)
		return PGC_RISK_CRITICAL;
	if (score >= 60)
		return PGC_RISK_HIGH;
	if (score >= 40)
		return PGC_RISK_MEDIUM;
	if (score >= 20)
		return PGC_RISK_LOW;
	return PGC_RISK_INFO;
}

const char *
pg_circuit_mode_name(int mode)
{
	switch (mode)
	{
		case PGC_MODE_OBSERVE:
			return "observe";
		case PGC_MODE_WARN:
			return "warn";
		case PGC_MODE_ENFORCE:
			return "enforce";
		default:
			return "unknown";
	}
}

const char *
pg_circuit_safety_mode_name(PgCircuitSafetyMode mode)
{
	switch (mode)
	{
		case PGC_SAFETY_NORMAL:
			return "NORMAL";
		case PGC_SAFETY_PROTECT:
			return "PROTECT";
		case PGC_SAFETY_EMERGENCY:
			return "EMERGENCY";
		default:
			return "UNKNOWN";
	}
}

const char *
pg_circuit_runtime_cfg_name(int mode)
{
	switch (mode)
	{
		case PGC_RUNTIME_CFG_NORMAL:
			return "normal";
		case PGC_RUNTIME_CFG_PROTECT:
			return "protect";
		case PGC_RUNTIME_CFG_EMERGENCY:
			return "emergency";
		case PGC_RUNTIME_CFG_AUTO:
			return "auto";
		default:
			return "unknown";
	}
}

const char *
pg_circuit_decision_name(PgCircuitDecision decision)
{
	switch (decision)
	{
		case PGC_DECISION_ALLOW:
			return "ALLOW";
		case PGC_DECISION_WARN:
			return "WARN";
		case PGC_DECISION_BLOCK:
			return "BLOCK";
		default:
			return "UNKNOWN";
	}
}

const char *
pg_circuit_risk_level_name(PgCircuitRiskLevel level)
{
	switch (level)
	{
		case PGC_RISK_INFO:
			return "INFO";
		case PGC_RISK_LOW:
			return "LOW";
		case PGC_RISK_MEDIUM:
			return "MEDIUM";
		case PGC_RISK_HIGH:
			return "HIGH";
		case PGC_RISK_CRITICAL:
			return "CRITICAL";
		default:
			return "UNKNOWN";
	}
}

const char *
pg_circuit_workload_class_name(PgCircuitWorkloadClass wclass)
{
	switch (wclass)
	{
		case PGC_WCLASS_READ:
			return "read";
		case PGC_WCLASS_OLTP_SMALL:
			return "oltp_small";
		case PGC_WCLASS_BULK_WRITE:
			return "bulk_write";
		case PGC_WCLASS_DDL:
			return "ddl";
		case PGC_WCLASS_MAINTENANCE:
			return "maintenance";
		default:
			return "unknown";
	}
}
