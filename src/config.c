/*-------------------------------------------------------------------------
 *
 * config.c
 *		GUC configuration for PG Circuit Community
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "utils/guc.h"

#include "pg_circuit.h"

bool		pg_circuit_enabled = true;
bool		pg_circuit_debug = false;
bool		pg_circuit_fail_closed = false;
int			pg_circuit_mode = PGC_MODE_WARN;
#ifdef PGCIRCUIT_FAULT_INJECTION
char	   *pg_circuit_fault = NULL;
#endif
int			pg_circuit_runtime_mode = PGC_RUNTIME_CFG_NORMAL;
int			pg_circuit_long_transaction_seconds = 300;
int			pg_circuit_replication_lag_warning_seconds = 10;
int			pg_circuit_replication_lag_critical_seconds = 30;
int			pg_circuit_risk_warn_threshold = 50;
int			pg_circuit_risk_block_threshold = 80;
int			pg_circuit_event_history_size = 256;
int			pg_circuit_wal_pressure_elevated_bps = 16 * 1024 * 1024;
int			pg_circuit_wal_pressure_high_bps = 64 * 1024 * 1024;

static const struct config_enum_entry mode_options[] = {
	{"observe", PGC_MODE_OBSERVE, false},
	{"warn", PGC_MODE_WARN, false},
	{"enforce", PGC_MODE_ENFORCE, false},
	{NULL, 0, false}
};

/*
 * Keep the enum for compatibility with docs/tests that SET the GUC, but
 * Community always resolves safety mode to NORMAL regardless of value.
 */
static const struct config_enum_entry runtime_mode_options[] = {
	{"normal", PGC_RUNTIME_CFG_NORMAL, false},
	{"protect", PGC_RUNTIME_CFG_PROTECT, false},
	{"emergency", PGC_RUNTIME_CFG_EMERGENCY, false},
	{"auto", PGC_RUNTIME_CFG_AUTO, false},
	{NULL, 0, false}
};

void
pg_circuit_register_gucs(void)
{
	DefineCustomBoolVariable("pg_circuit.enabled",
							 "Enable PG Circuit runtime protection.",
							 NULL,
							 &pg_circuit_enabled,
							 true,
							 PGC_SUSET,
							 0,
							 NULL, NULL, NULL);

	DefineCustomBoolVariable("pg_circuit.debug",
							 "Log PG Circuit risk decisions at DEBUG1.",
							 NULL,
							 &pg_circuit_debug,
							 false,
							 PGC_SUSET,
							 0,
							 NULL, NULL, NULL);

	DefineCustomEnumVariable("pg_circuit.mode",
							 "Operating mode: observe, warn, or enforce.",
							 NULL,
							 &pg_circuit_mode,
							 PGC_MODE_WARN,
							 mode_options,
							 PGC_SUSET,
							 0,
							 NULL, NULL, NULL);

	DefineCustomEnumVariable("pg_circuit.runtime_mode",
							 "Runtime safety mode (Community: always NORMAL).",
							 NULL,
							 &pg_circuit_runtime_mode,
							 PGC_RUNTIME_CFG_NORMAL,
							 runtime_mode_options,
							 PGC_SUSET,
							 0,
							 NULL, NULL, NULL);

	DefineCustomIntVariable("pg_circuit.long_transaction_seconds",
							"Age in seconds after which a transaction is considered long-running.",
							NULL,
							&pg_circuit_long_transaction_seconds,
							300,
							1, INT_MAX,
							PGC_SUSET,
							0,
							NULL, NULL, NULL);

	DefineCustomIntVariable("pg_circuit.replication_lag_warning_seconds",
							"Replication lag in seconds that raises a warning contribution.",
							NULL,
							&pg_circuit_replication_lag_warning_seconds,
							10,
							0, INT_MAX,
							PGC_SUSET,
							0,
							NULL, NULL, NULL);

	DefineCustomIntVariable("pg_circuit.replication_lag_critical_seconds",
							"Replication lag in seconds that raises a critical contribution.",
							NULL,
							&pg_circuit_replication_lag_critical_seconds,
							30,
							0, INT_MAX,
							PGC_SUSET,
							0,
							NULL, NULL, NULL);

	DefineCustomIntVariable("pg_circuit.risk_warn_threshold",
							"Risk score at or above which PG Circuit warns.",
							NULL,
							&pg_circuit_risk_warn_threshold,
							50,
							0, 100,
							PGC_SUSET,
							0,
							NULL, NULL, NULL);

	DefineCustomIntVariable("pg_circuit.risk_block_threshold",
							"Risk score at or above which PG Circuit blocks in enforce mode.",
							NULL,
							&pg_circuit_risk_block_threshold,
							80,
							0, 100,
							PGC_SUSET,
							0,
							NULL, NULL, NULL);

	DefineCustomIntVariable("pg_circuit.event_history_size",
							"Maximum WARN/BLOCK events retained in shared memory.",
							NULL,
							&pg_circuit_event_history_size,
							256,
							1, 4096,
							PGC_POSTMASTER,
							0,
							NULL, NULL, NULL);

	DefineCustomIntVariable("pg_circuit.wal_pressure_elevated_bps",
							"WAL bytes/second that marks WAL pressure as elevated.",
							NULL,
							&pg_circuit_wal_pressure_elevated_bps,
							16 * 1024 * 1024,
							1, INT_MAX,
							PGC_SUSET,
							0,
							NULL, NULL, NULL);

	DefineCustomIntVariable("pg_circuit.wal_pressure_high_bps",
							"WAL bytes/second that marks WAL pressure as high.",
							NULL,
							&pg_circuit_wal_pressure_high_bps,
							64 * 1024 * 1024,
							1, INT_MAX,
							PGC_SUSET,
							0,
							NULL, NULL, NULL);

	DefineCustomBoolVariable("pg_circuit.fail_closed",
							 "In enforce mode, treat internal PG Circuit errors as BLOCK. "
							 "observe/warn always fail open.",
							 NULL,
							 &pg_circuit_fail_closed,
							 false,
							 PGC_SUSET,
							 0,
							 NULL, NULL, NULL);

#ifdef PGCIRCUIT_FAULT_INJECTION
	DefineCustomStringVariable("pg_circuit.fault",
							   "Developer fault injection. Empty disables. Not for production.",
							   NULL,
							   &pg_circuit_fault,
							   "",
							   PGC_SUSET,
							   0,
							   NULL, NULL, NULL);
#endif

	EmitWarningsOnPlaceholders("pg_circuit");
}
