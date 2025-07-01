/*-------------------------------------------------------------------------
 *
 * risk_engine.c
 *		Deterministic, explainable risk scoring (Community)
 *
 * Community rules: PGC001–PGC005, PGC012–PGC016
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "lib/stringinfo.h"
#include "nodes/pg_list.h"
#include "utils/builtins.h"
#include "utils/memutils.h"

#include "pg_circuit.h"
#include "risk_engine.h"

void
pg_circuit_init_assessment(PgCircuitRiskAssessment *assessment)
{
	MemSet(assessment, 0, sizeof(*assessment));
	assessment->findings = NIL;
	assessment->decision = PGC_DECISION_ALLOW;
	assessment->primary_rule[0] = '\0';
}

void
pg_circuit_free_assessment(PgCircuitRiskAssessment *assessment)
{
	ListCell   *lc;

	foreach(lc, assessment->findings)
	{
		PgCircuitFinding *finding = (PgCircuitFinding *) lfirst(lc);

		pfree(finding);
	}

	list_free(assessment->findings);
	assessment->findings = NIL;
}

PgCircuitFinding *
pg_circuit_add_finding(PgCircuitRiskAssessment *assessment,
					   const char *rule_id,
					   int score_contribution,
					   const char *reason,
					   Oid relation_oid,
					   const char *relation_name,
					   double estimated_rows,
					   int64 relation_size)
{
	PgCircuitFinding *finding = palloc0(sizeof(PgCircuitFinding));

	strlcpy(finding->rule_id, rule_id, sizeof(finding->rule_id));
	finding->score_contribution = score_contribution;
	finding->level = pg_circuit_score_to_level(score_contribution);
	strlcpy(finding->reason, reason, sizeof(finding->reason));
	finding->relation_oid = relation_oid;
	if (relation_name != NULL)
		strlcpy(finding->relation_name, relation_name, sizeof(finding->relation_name));
	finding->estimated_rows = estimated_rows;
	finding->relation_size = relation_size;

	assessment->findings = lappend(assessment->findings, finding);
	assessment->score += score_contribution;

	if (assessment->primary_rule[0] == '\0' ||
		score_contribution >= 80)
		strlcpy(assessment->primary_rule, rule_id, sizeof(assessment->primary_rule));

	return finding;
}

int
pg_circuit_clamp_score(int score)
{
	if (score < 0)
		return 0;
	if (score > 100)
		return 100;
	return score;
}

int
pg_circuit_score_relation_size(int64 relation_size_bytes)
{
	const int64 gb = (int64) 1024 * 1024 * 1024;

	if (relation_size_bytes < 0)
		return 0;
	if (relation_size_bytes >= gb * 100)
		return 15;
	if (relation_size_bytes >= gb * 10)
		return 10;
	if (relation_size_bytes >= gb)
		return 5;
	return 0;
}

PgCircuitWorkloadClass
pg_circuit_classify_workload(const PgCircuitQueryInfo *qinfo)
{
	switch (qinfo->kind)
	{
		case PGC_QKIND_VACUUM_FULL:
		case PGC_QKIND_CLUSTER:
		case PGC_QKIND_REINDEX:
			return PGC_WCLASS_MAINTENANCE;
		case PGC_QKIND_ALTER_TABLE:
		case PGC_QKIND_CREATE_INDEX:
		case PGC_QKIND_CREATE_INDEX_CONCURRENTLY:
		case PGC_QKIND_DROP_TABLE:
		case PGC_QKIND_DROP_DATABASE:
		case PGC_QKIND_TRUNCATE:
			return PGC_WCLASS_DDL;
		case PGC_QKIND_DELETE_NO_WHERE:
		case PGC_QKIND_UPDATE_NO_WHERE:
			return PGC_WCLASS_BULK_WRITE;
		case PGC_QKIND_OTHER:
		default:
			if (qinfo->is_write)
				return PGC_WCLASS_OLTP_SMALL;
			return PGC_WCLASS_READ;
	}
}

const char *
pg_circuit_lock_class_name(PgCircuitLockClass lock_class)
{
	switch (lock_class)
	{
		case PGC_LOCK_NONE:
			return "none";
		case PGC_LOCK_LOW:
			return "low";
		case PGC_LOCK_MEDIUM:
			return "medium";
		case PGC_LOCK_HIGH:
			return "high";
		default:
			return "unknown";
	}
}

void
pg_circuit_finalize_assessment(PgCircuitRiskAssessment *assessment)
{
	int			warn_at;
	int			block_at;

	assessment->score = pg_circuit_clamp_score(assessment->score);
	assessment->level = pg_circuit_score_to_level(assessment->score);

	pg_circuit_effective_risk_thresholds(&warn_at, &block_at);
	assessment->should_warn = (assessment->score >= warn_at);
	assessment->should_block = (assessment->score >= block_at);
	assessment->decision = PGC_DECISION_ALLOW;

	switch (pg_circuit_mode)
	{
		case PGC_MODE_OBSERVE:
			assessment->decision = PGC_DECISION_ALLOW;
			break;
		case PGC_MODE_WARN:
			if (assessment->should_warn)
				assessment->decision = PGC_DECISION_WARN;
			break;
		case PGC_MODE_ENFORCE:
			if (assessment->should_block)
				assessment->decision = PGC_DECISION_BLOCK;
			else if (assessment->should_warn)
				assessment->decision = PGC_DECISION_WARN;
			break;
		default:
			break;
	}
}

char *
pg_circuit_format_findings(const PgCircuitRiskAssessment *assessment)
{
	StringInfoData buf;
	ListCell   *lc;

	initStringInfo(&buf);
	foreach(lc, assessment->findings)
	{
		PgCircuitFinding *f = (PgCircuitFinding *) lfirst(lc);

		appendStringInfo(&buf, "%s %s +%d\n",
						 f->rule_id, f->reason, f->score_contribution);
	}
	appendStringInfo(&buf, "Final score: %d/100", assessment->score);
	return buf.data;
}

bool
pg_circuit_parse_query_kind(const char *name, PgCircuitQueryKind *kind)
{
	if (name == NULL)
		return false;
	if (pg_strcasecmp(name, "delete_no_where") == 0)
		*kind = PGC_QKIND_DELETE_NO_WHERE;
	else if (pg_strcasecmp(name, "update_no_where") == 0)
		*kind = PGC_QKIND_UPDATE_NO_WHERE;
	else if (pg_strcasecmp(name, "truncate") == 0)
		*kind = PGC_QKIND_TRUNCATE;
	else if (pg_strcasecmp(name, "drop_table") == 0)
		*kind = PGC_QKIND_DROP_TABLE;
	else if (pg_strcasecmp(name, "drop_database") == 0)
		*kind = PGC_QKIND_DROP_DATABASE;
	else if (pg_strcasecmp(name, "alter_table") == 0)
		*kind = PGC_QKIND_ALTER_TABLE;
	else if (pg_strcasecmp(name, "create_index") == 0)
		*kind = PGC_QKIND_CREATE_INDEX;
	else if (pg_strcasecmp(name, "create_index_concurrently") == 0)
		*kind = PGC_QKIND_CREATE_INDEX_CONCURRENTLY;
	else if (pg_strcasecmp(name, "reindex") == 0)
		*kind = PGC_QKIND_REINDEX;
	else if (pg_strcasecmp(name, "vacuum_full") == 0)
		*kind = PGC_QKIND_VACUUM_FULL;
	else if (pg_strcasecmp(name, "cluster") == 0)
		*kind = PGC_QKIND_CLUSTER;
	else if (pg_strcasecmp(name, "other") == 0)
		*kind = PGC_QKIND_OTHER;
	else
		return false;
	return true;
}

bool
pg_circuit_parse_safety_mode(const char *name, PgCircuitSafetyMode *mode)
{
	if (name == NULL)
		return false;
	if (pg_strcasecmp(name, "normal") == 0)
		*mode = PGC_SAFETY_NORMAL;
	else if (pg_strcasecmp(name, "protect") == 0)
		*mode = PGC_SAFETY_PROTECT;
	else if (pg_strcasecmp(name, "emergency") == 0)
		*mode = PGC_SAFETY_EMERGENCY;
	else
		return false;
	return true;
}

void
pg_circuit_assess_query(const PgCircuitQueryInfo *qinfo,
						const PgCircuitRuntimeState *runtime,
						PgCircuitRiskAssessment *assessment)
{
	int			contrib;
	char		reason[256];
	const char *relname = qinfo->relation_name;
	int64		relsize = qinfo->relation_size;

	(void) runtime;

	pg_circuit_init_assessment(assessment);

	switch (qinfo->kind)
	{
		case PGC_QKIND_DELETE_NO_WHERE:
			pg_circuit_add_finding(assessment, "PGC001", 95,
								   "DELETE without WHERE clause",
								   qinfo->relation_oid, relname,
								   qinfo->estimated_rows, relsize);
			break;

		case PGC_QKIND_UPDATE_NO_WHERE:
			pg_circuit_add_finding(assessment, "PGC002", 95,
								   "UPDATE without WHERE clause",
								   qinfo->relation_oid, relname,
								   qinfo->estimated_rows, relsize);
			break;

		case PGC_QKIND_TRUNCATE:
			pg_circuit_add_finding(assessment, "PGC003", 90,
								   "TRUNCATE statement",
								   qinfo->relation_oid, relname,
								   qinfo->estimated_rows, relsize);
			break;

		case PGC_QKIND_DROP_TABLE:
			pg_circuit_add_finding(assessment, "PGC004", 95,
								   "DROP TABLE statement",
								   qinfo->relation_oid, relname,
								   -1, relsize);
			break;

		case PGC_QKIND_DROP_DATABASE:
			pg_circuit_add_finding(assessment, "PGC005", 100,
								   "DROP DATABASE statement",
								   InvalidOid, relname, -1, -1);
			break;

		case PGC_QKIND_ALTER_TABLE:
			{
				int			base = 35;

				if (qinfo->lock_class >= PGC_LOCK_HIGH)
					base = 60;
				else if (qinfo->lock_class >= PGC_LOCK_MEDIUM)
					base = 45;
				snprintf(reason, sizeof(reason),
						 "ALTER TABLE high-lock-risk operation (%s, lock class %s%s)",
						 qinfo->ddl_detail[0] ? qinfo->ddl_detail : "ALTER TABLE",
						 pg_circuit_lock_class_name(qinfo->lock_class),
						 qinfo->rewrite_likely ? ", rewrite likely" : "");
				pg_circuit_add_finding(assessment, "PGC012", base, reason,
									   qinfo->relation_oid, relname,
									   qinfo->estimated_rows, relsize);
				break;
			}

		case PGC_QKIND_CREATE_INDEX:
			contrib = (relsize >= 0 && relsize < ((int64) 64 * 1024 * 1024)) ? 25 : 50;
			pg_circuit_add_finding(assessment, "PGC013", contrib,
								   "CREATE INDEX without CONCURRENTLY",
								   qinfo->relation_oid, relname,
								   qinfo->estimated_rows, relsize);
			break;

		case PGC_QKIND_CREATE_INDEX_CONCURRENTLY:
			pg_circuit_add_finding(assessment, "PGC013", 15,
								   "CREATE INDEX CONCURRENTLY (lower lock impact)",
								   qinfo->relation_oid, relname,
								   qinfo->estimated_rows, relsize);
			break;

		case PGC_QKIND_REINDEX:
			if (qinfo->concurrent)
				pg_circuit_add_finding(assessment, "PGC014", 20,
									   "REINDEX CONCURRENTLY",
									   qinfo->relation_oid, relname,
									   qinfo->estimated_rows, relsize);
			else
				pg_circuit_add_finding(assessment, "PGC014", 55,
									   "REINDEX without CONCURRENTLY",
									   qinfo->relation_oid, relname,
									   qinfo->estimated_rows, relsize);
			break;

		case PGC_QKIND_VACUUM_FULL:
			pg_circuit_add_finding(assessment, "PGC015", 70,
								   "VACUUM FULL on relation",
								   qinfo->relation_oid, relname,
								   qinfo->estimated_rows, relsize);
			break;

		case PGC_QKIND_CLUSTER:
			pg_circuit_add_finding(assessment, "PGC016", 70,
								   "CLUSTER on relation",
								   qinfo->relation_oid, relname,
								   qinfo->estimated_rows, relsize);
			break;

		case PGC_QKIND_OTHER:
		default:
			pg_circuit_finalize_assessment(assessment);
			return;
	}

	pg_circuit_finalize_assessment(assessment);
}
