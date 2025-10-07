/*-------------------------------------------------------------------------
 *
 * event.c
 *		Risk-event fill and shared-memory persistence
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "commands/dbcommands.h"
#include "miscadmin.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/timestamp.h"

#include "event.h"
#include "shmem.h"

const char *
pg_circuit_query_kind_name(PgCircuitQueryKind kind)
{
	switch (kind)
	{
		case PGC_QKIND_DELETE_NO_WHERE:
			return "delete_no_where";
		case PGC_QKIND_UPDATE_NO_WHERE:
			return "update_no_where";
		case PGC_QKIND_TRUNCATE:
			return "truncate";
		case PGC_QKIND_DROP_TABLE:
			return "drop_table";
		case PGC_QKIND_DROP_DATABASE:
			return "drop_database";
		case PGC_QKIND_ALTER_TABLE:
			return "alter_table";
		case PGC_QKIND_CREATE_INDEX:
			return "create_index";
		case PGC_QKIND_CREATE_INDEX_CONCURRENTLY:
			return "create_index_concurrently";
		case PGC_QKIND_REINDEX:
			return "reindex";
		case PGC_QKIND_VACUUM_FULL:
			return "vacuum_full";
		case PGC_QKIND_CLUSTER:
			return "cluster";
		case PGC_QKIND_OTHER:
		default:
			return "other";
	}
}

void
pg_circuit_fill_risk_event(PgCircuitRiskEvent *event,
						   const PgCircuitQueryInfo *qinfo,
						   const PgCircuitRuntimeState *runtime,
						   const PgCircuitRiskAssessment *assessment,
						   const PgCircuitPolicyResult *policy)
{
	MemSet(event, 0, sizeof(*event));
	event->timestamp = GetCurrentTimestamp();
	event->backend_pid = MyProcPid;
	event->database_oid = MyDatabaseId;
	event->user_oid = GetUserId();
	strlcpy(event->primary_rule, assessment->primary_rule,
			sizeof(event->primary_rule));
	event->risk_score = assessment->score;
	event->decision = assessment->decision;
	event->base_decision = policy ? policy->base_decision : assessment->decision;
	event->runtime_mode = runtime->mode;
	event->relation_oid = qinfo->relation_oid;
	event->operation = qinfo->kind;
}

static void
build_rule_ids(const PgCircuitRiskAssessment *assessment,
			   char *buf, size_t buflen)
{
	ListCell   *lc;
	size_t		used = 0;

	buf[0] = '\0';
	foreach(lc, assessment->findings)
	{
		PgCircuitFinding *f = (PgCircuitFinding *) lfirst(lc);
		size_t		need;

		if (f->rule_id[0] == '\0')
			continue;
		need = strlen(f->rule_id) + (used > 0 ? 1 : 0);
		if (used + need + 1 >= buflen)
			break;
		if (used > 0)
			buf[used++] = ',';
		memcpy(buf + used, f->rule_id, strlen(f->rule_id));
		used += strlen(f->rule_id);
		buf[used] = '\0';
	}
	if (buf[0] == '\0' && assessment->primary_rule[0])
		strlcpy(buf, assessment->primary_rule, buflen);
}

void
pg_circuit_persist_risk_event(const PgCircuitRiskEvent *event,
							  const PgCircuitQueryInfo *qinfo,
							  const PgCircuitRiskAssessment *assessment,
							  const PgCircuitPolicyResult *policy)
{
	char		rule_ids[PGC_RULE_IDS_MAX];
	char		dbname[NAMEDATALEN];
	char		username[NAMEDATALEN];
	const char *relname;
	char	   *tmp;

	build_rule_ids(assessment, rule_ids, sizeof(rule_ids));

	dbname[0] = '\0';
	tmp = get_database_name(event->database_oid);
	if (tmp)
	{
		strlcpy(dbname, tmp, sizeof(dbname));
		pfree(tmp);
	}

	username[0] = '\0';
	tmp = GetUserNameFromId(event->user_oid, true);
	if (tmp)
	{
		strlcpy(username, tmp, sizeof(username));
		pfree(tmp);
	}

	if (qinfo->relation_qualname[0])
		relname = qinfo->relation_qualname;
	else
		relname = qinfo->relation_name;

	pg_circuit_event_record(event, rule_ids, relname, dbname, username, policy);
}
