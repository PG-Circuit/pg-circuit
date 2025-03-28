/*-------------------------------------------------------------------------
 *
 * risk_engine.h
 *		Deterministic risk scoring for PG Circuit
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_CIRCUIT_RISK_ENGINE_H
#define PG_CIRCUIT_RISK_ENGINE_H

#include "pg_circuit.h"
#include "runtime_state.h"

typedef struct PgCircuitFinding
{
	char		rule_id[16];
	PgCircuitRiskLevel level;
	int			score_contribution;
	char		reason[256];
	char		title[64];			/* optional */
	char		remediation[128];	/* optional hint */
	Oid			relation_oid;
	char		relation_name[NAMEDATALEN];
	double		estimated_rows;
	int64		relation_size;
} PgCircuitFinding;

typedef struct PgCircuitRiskAssessment
{
	int			score;
	PgCircuitRiskLevel level;
	bool		should_warn;
	bool		should_block;
	PgCircuitDecision decision;
	List	   *findings;		/* List of PgCircuitFinding* */
	char		primary_rule[16];
} PgCircuitRiskAssessment;

typedef enum PgCircuitQueryKind
{
	PGC_QKIND_OTHER = 0,
	PGC_QKIND_DELETE_NO_WHERE,
	PGC_QKIND_UPDATE_NO_WHERE,
	PGC_QKIND_TRUNCATE,
	PGC_QKIND_DROP_TABLE,
	PGC_QKIND_DROP_DATABASE,
	PGC_QKIND_ALTER_TABLE,
	PGC_QKIND_CREATE_INDEX,
	PGC_QKIND_CREATE_INDEX_CONCURRENTLY,
	PGC_QKIND_REINDEX,
	PGC_QKIND_VACUUM_FULL,
	PGC_QKIND_CLUSTER
} PgCircuitQueryKind;

/*
 * Conservative lock-impact class. Exact PostgreSQL lock mode can vary by
 * subcommand form and version — we document uncertainty in findings.
 */
typedef enum PgCircuitLockClass
{
	PGC_LOCK_NONE = 0,
	PGC_LOCK_LOW,
	PGC_LOCK_MEDIUM,
	PGC_LOCK_HIGH				/* likely ACCESS EXCLUSIVE or rewrite */
} PgCircuitLockClass;

typedef struct PgCircuitQueryInfo
{
	PgCircuitQueryKind kind;
	Oid			relation_oid;
	char		relation_name[NAMEDATALEN];
	char		relation_qualname[NAMEDATALEN * 2 + 2];
	double		estimated_rows;
	int64		relation_size;
	bool		has_where;
	bool		is_write;
	bool		concurrent;
	bool		rewrite_likely;
	PgCircuitLockClass lock_class;
	PgCircuitWorkloadClass workload_class;
	char		ddl_detail[128];
} PgCircuitQueryInfo;

extern void pg_circuit_init_assessment(PgCircuitRiskAssessment *assessment);
extern void pg_circuit_free_assessment(PgCircuitRiskAssessment *assessment);
extern PgCircuitFinding *pg_circuit_add_finding(PgCircuitRiskAssessment *assessment,
												const char *rule_id,
												int score_contribution,
												const char *reason,
												Oid relation_oid,
												const char *relation_name,
												double estimated_rows,
												int64 relation_size);
extern void pg_circuit_finalize_assessment(PgCircuitRiskAssessment *assessment);
extern void pg_circuit_assess_query(const PgCircuitQueryInfo *qinfo,
									const PgCircuitRuntimeState *runtime,
									PgCircuitRiskAssessment *assessment);

extern char *pg_circuit_format_findings(const PgCircuitRiskAssessment *assessment);

extern int	pg_circuit_score_relation_size(int64 relation_size_bytes);
extern PgCircuitWorkloadClass pg_circuit_classify_workload(const PgCircuitQueryInfo *qinfo);
extern int	pg_circuit_clamp_score(int score);

extern bool pg_circuit_parse_query_kind(const char *name, PgCircuitQueryKind *kind);
extern bool pg_circuit_parse_safety_mode(const char *name, PgCircuitSafetyMode *mode);
extern const char *pg_circuit_lock_class_name(PgCircuitLockClass lock_class);

#endif							/* PG_CIRCUIT_RISK_ENGINE_H */
