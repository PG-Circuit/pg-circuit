/*-------------------------------------------------------------------------
 *
 * pg_circuit.h
 *		Adaptive runtime protection for PostgreSQL (Community)
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_CIRCUIT_H
#define PG_CIRCUIT_H

#include "postgres.h"

#include "nodes/execnodes.h"
#include "nodes/parsenodes.h"
#include "nodes/plannodes.h"
#include "tcop/utility.h"
#include "utils/guc.h"

#define PG_CIRCUIT_VERSION "0.1.0"
#define PG_CIRCUIT_VERSION_NUM 100

/* Shared-memory layout version — bump when PgCircuitSharedState shape changes */
#define PG_CIRCUIT_SHMEM_MAGIC 0x50474339	/* 'PGC9' */
#define PG_CIRCUIT_SHMEM_VERSION 5

/* Operating modes */
typedef enum PgCircuitMode
{
	PGC_MODE_OBSERVE = 0,
	PGC_MODE_WARN = 1,
	PGC_MODE_ENFORCE = 2
} PgCircuitMode;

/* Runtime safety modes (Community always resolves to NORMAL) */
typedef enum PgCircuitSafetyMode
{
	PGC_SAFETY_NORMAL = 0,
	PGC_SAFETY_PROTECT = 1,
	PGC_SAFETY_EMERGENCY = 2
} PgCircuitSafetyMode;

/* Runtime mode configuration (Community ignores non-normal) */
typedef enum PgCircuitRuntimeModeConfig
{
	PGC_RUNTIME_CFG_NORMAL = 0,
	PGC_RUNTIME_CFG_PROTECT = 1,
	PGC_RUNTIME_CFG_EMERGENCY = 2,
	PGC_RUNTIME_CFG_AUTO = 3
} PgCircuitRuntimeModeConfig;

/* Workload class for circuit-breaker behavior */
typedef enum PgCircuitWorkloadClass
{
	PGC_WCLASS_READ = 0,
	PGC_WCLASS_OLTP_SMALL = 1,
	PGC_WCLASS_BULK_WRITE = 2,
	PGC_WCLASS_DDL = 3,
	PGC_WCLASS_MAINTENANCE = 4
} PgCircuitWorkloadClass;

/* Risk levels */
typedef enum PgCircuitRiskLevel
{
	PGC_RISK_INFO = 0,
	PGC_RISK_LOW = 1,
	PGC_RISK_MEDIUM = 2,
	PGC_RISK_HIGH = 3,
	PGC_RISK_CRITICAL = 4
} PgCircuitRiskLevel;

/* Decision outcomes */
typedef enum PgCircuitDecision
{
	PGC_DECISION_ALLOW = 0,
	PGC_DECISION_WARN = 1,
	PGC_DECISION_BLOCK = 2
} PgCircuitDecision;

/* Forward declarations */
struct PgCircuitRuntimeState;
struct PgCircuitRiskAssessment;
struct PgCircuitFinding;

/* GUC variables — defined in config.c */
extern bool pg_circuit_enabled;
extern bool pg_circuit_debug;
extern bool pg_circuit_fail_closed;
extern int	pg_circuit_mode;
extern int	pg_circuit_runtime_mode;
extern int	pg_circuit_long_transaction_seconds;
extern int	pg_circuit_replication_lag_warning_seconds;
extern int	pg_circuit_replication_lag_critical_seconds;
extern int	pg_circuit_risk_warn_threshold;
extern int	pg_circuit_risk_block_threshold;
extern int	pg_circuit_event_history_size;
extern int	pg_circuit_wal_pressure_elevated_bps;
extern int	pg_circuit_wal_pressure_high_bps;

/* Module lifecycle */
extern void pg_circuit_register_gucs(void);
extern void pg_circuit_install_hooks(void);
extern void pg_circuit_uninstall_hooks(void);

/* Helpers */
extern PgCircuitRiskLevel pg_circuit_score_to_level(int score);
extern const char *pg_circuit_mode_name(int mode);
extern const char *pg_circuit_safety_mode_name(PgCircuitSafetyMode mode);
extern const char *pg_circuit_runtime_cfg_name(int mode);
extern const char *pg_circuit_decision_name(PgCircuitDecision decision);
extern const char *pg_circuit_risk_level_name(PgCircuitRiskLevel level);
extern const char *pg_circuit_workload_class_name(PgCircuitWorkloadClass wclass);

#endif							/* PG_CIRCUIT_H */
