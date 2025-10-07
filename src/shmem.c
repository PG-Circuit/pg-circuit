/*-------------------------------------------------------------------------
 *
 * shmem.c
 *		Shared-memory event ring and WAL rate cache (Community)
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "common/md5.h"
#include "miscadmin.h"
#include "storage/ipc.h"
#include "storage/lwlock.h"
#include "storage/shmem.h"
#include "utils/guc.h"

#include "event.h"
#include "fault.h"
#include "pg_circuit.h"
#include "shmem.h"

PgCircuitSharedState *pg_circuit_shared = NULL;

static shmem_request_hook_type prev_shmem_request_hook = NULL;
static shmem_startup_hook_type prev_shmem_startup_hook = NULL;

static void
fingerprint_op(PgCircuitQueryKind kind, const char *relation_name, char *out33)
{
	char		src[128];
	const char *kname = pg_circuit_query_kind_name(kind);
	const char *errstr = NULL;

	snprintf(src, sizeof(src), "%s|%s",
			 kname ? kname : "?",
			 relation_name ? relation_name : "");
	if (!pg_md5_hash(src, strlen(src), out33, &errstr))
		out33[0] = '\0';
}

Size
pg_circuit_shared_memsize(void)
{
	int			cap = pg_circuit_event_history_size;

	if (cap < 1)
		cap = 1;
	return offsetof(PgCircuitSharedState, events) +
		(Size) cap * sizeof(PgCircuitSharedEvent);
}

static void
pg_circuit_shmem_request(void)
{
	if (prev_shmem_request_hook)
		prev_shmem_request_hook();

	RequestAddinShmemSpace(pg_circuit_shared_memsize());
	RequestNamedLWLockTranche("pg_circuit", 1);
}

static void
pg_circuit_shmem_startup(void)
{
	bool		found;
	int			cap;

	if (prev_shmem_startup_hook)
		prev_shmem_startup_hook();

	cap = pg_circuit_event_history_size;
	if (cap < 1)
		cap = 1;

	LWLockAcquire(AddinShmemInitLock, LW_EXCLUSIVE);
	pg_circuit_shared = (PgCircuitSharedState *)
		ShmemInitStruct("pg_circuit",
						pg_circuit_shared_memsize(),
						&found);
	if (!found)
	{
		MemSet(pg_circuit_shared, 0, pg_circuit_shared_memsize());
		pg_circuit_shared->magic = PG_CIRCUIT_SHMEM_MAGIC;
		pg_circuit_shared->version = PG_CIRCUIT_SHMEM_VERSION;
		pg_circuit_shared->lock =
			&(GetNamedLWLockTranche("pg_circuit"))->lock;
		pg_circuit_shared->capacity = cap;
		pg_circuit_shared->head = 0;
		pg_circuit_shared->count = 0;
		pg_circuit_shared->wal.valid = false;
		pg_circuit_shared->next_event_id = 0;
		pg_circuit_shared->next_decision_id = 0;
	}
	else
	{
		if (pg_circuit_shared->magic != PG_CIRCUIT_SHMEM_MAGIC ||
			pg_circuit_shared->version != PG_CIRCUIT_SHMEM_VERSION)
		{
			elog(LOG,
				 "pg_circuit: shared memory version mismatch (got magic=%u ver=%u, want %u/%u); "
				 "event history disabled until restart with matching module",
				 pg_circuit_shared->magic, pg_circuit_shared->version,
				 PG_CIRCUIT_SHMEM_MAGIC, PG_CIRCUIT_SHMEM_VERSION);
			pg_circuit_shared = NULL;
		}
		else if (pg_circuit_shared->lock == NULL)
		{
			pg_circuit_shared->lock =
				&(GetNamedLWLockTranche("pg_circuit"))->lock;
		}
	}
	LWLockRelease(AddinShmemInitLock);
}

void
pg_circuit_shmem_register(void)
{
	prev_shmem_request_hook = shmem_request_hook;
	shmem_request_hook = pg_circuit_shmem_request;
	prev_shmem_startup_hook = shmem_startup_hook;
	shmem_startup_hook = pg_circuit_shmem_startup;
}

void
pg_circuit_event_record(const PgCircuitRiskEvent *event,
						const char *rule_ids,
						const char *relation_name,
						const char *database_name,
						const char *user_name,
						const PgCircuitPolicyResult *policy)
{
	PgCircuitSharedEvent *slot;
	int64		event_id = 0;
	int64		decision_id = 0;
	char		fingerprint[33];

	if (pg_circuit_shared == NULL || event == NULL)
		return;

	if (pg_circuit_fault_is("event_full"))
		return;

	/* Bound history: only persist warn/block decisions */
	if (event->decision == PGC_DECISION_ALLOW &&
		!(policy && policy->audit_only))
		return;

	fingerprint[0] = '\0';
	fingerprint_op((PgCircuitQueryKind) event->operation,
				   relation_name, fingerprint);

	LWLockAcquire(pg_circuit_shared->lock, LW_EXCLUSIVE);

	if (pg_circuit_shared->capacity < 1)
	{
		LWLockRelease(pg_circuit_shared->lock);
		return;
	}

	event_id = ++pg_circuit_shared->next_event_id;
	decision_id = ++pg_circuit_shared->next_decision_id;

	slot = &pg_circuit_shared->events[pg_circuit_shared->head];
	MemSet(slot, 0, sizeof(*slot));
	slot->timestamp = event->timestamp;
	slot->event_id = event_id;
	slot->decision_id = decision_id;
	slot->backend_pid = event->backend_pid;
	slot->database_oid = event->database_oid;
	slot->user_oid = event->user_oid;
	slot->relation_oid = event->relation_oid;
	slot->decision = (int16) event->decision;
	slot->base_decision = (int16) event->base_decision;
	slot->runtime_mode = (int16) event->runtime_mode;
	slot->operation = (int16) event->operation;
	slot->risk_score = event->risk_score;
	strlcpy(slot->primary_rule, event->primary_rule, sizeof(slot->primary_rule));
	if (rule_ids)
		strlcpy(slot->rule_ids, rule_ids, sizeof(slot->rule_ids));
	else
		strlcpy(slot->rule_ids, event->primary_rule, sizeof(slot->rule_ids));
	if (relation_name)
		strlcpy(slot->relation_name, relation_name, sizeof(slot->relation_name));
	if (database_name)
		strlcpy(slot->database_name, database_name, sizeof(slot->database_name));
	if (user_name)
		strlcpy(slot->user_name, user_name, sizeof(slot->user_name));
	strlcpy(slot->fingerprint, fingerprint, sizeof(slot->fingerprint));

	pg_circuit_shared->head =
		(pg_circuit_shared->head + 1) % pg_circuit_shared->capacity;
	if (pg_circuit_shared->count < pg_circuit_shared->capacity)
		pg_circuit_shared->count++;

	LWLockRelease(pg_circuit_shared->lock);
}

int
pg_circuit_event_snapshot(PgCircuitSharedEvent *out, int max_out)
{
	int			n;
	int			i;
	int			idx;

	if (pg_circuit_shared == NULL || out == NULL || max_out <= 0)
		return 0;

	LWLockAcquire(pg_circuit_shared->lock, LW_SHARED);

	n = Min(pg_circuit_shared->count, max_out);
	for (i = 0; i < n; i++)
	{
		idx = pg_circuit_shared->head - 1 - i;
		while (idx < 0)
			idx += pg_circuit_shared->capacity;
		out[i] = pg_circuit_shared->events[idx];
	}

	LWLockRelease(pg_circuit_shared->lock);
	return n;
}

void
pg_circuit_wal_sample_update(uint64 wal_bytes, TimestampTz now,
							 double *bytes_per_sec_out,
							 int *pressure_level_out)
{
	double		bps = 0;
	int			level = 0;
	long		usec;

	if (bytes_per_sec_out)
		*bytes_per_sec_out = 0;
	if (pressure_level_out)
		*pressure_level_out = 0;

	if (pg_circuit_shared == NULL)
		return;

	LWLockAcquire(pg_circuit_shared->lock, LW_EXCLUSIVE);

	if (pg_circuit_shared->wal.valid)
	{
		usec = now - pg_circuit_shared->wal.sample_time;
		if (usec >= 1000000L)
		{
			double		secs = (double) usec / 1000000.0;
			uint64		prev = pg_circuit_shared->wal.wal_bytes;

			if (wal_bytes >= prev && secs > 0)
				bps = (double) (wal_bytes - prev) / secs;
			else
				bps = 0;

			if (bps >= (double) pg_circuit_wal_pressure_high_bps)
				level = 3;
			else if (bps >= (double) pg_circuit_wal_pressure_elevated_bps)
				level = 2;
			else if (bps > 0)
				level = 1;
			else
				level = 0;

			pg_circuit_shared->wal.wal_bytes = wal_bytes;
			pg_circuit_shared->wal.sample_time = now;
			pg_circuit_shared->wal.bytes_per_sec = bps;
			pg_circuit_shared->wal.pressure_level = level;
		}
		else
		{
			bps = pg_circuit_shared->wal.bytes_per_sec;
			level = pg_circuit_shared->wal.pressure_level;
		}
	}
	else
	{
		pg_circuit_shared->wal.valid = true;
		pg_circuit_shared->wal.wal_bytes = wal_bytes;
		pg_circuit_shared->wal.sample_time = now;
		pg_circuit_shared->wal.bytes_per_sec = 0;
		pg_circuit_shared->wal.pressure_level = 0;
	}

	LWLockRelease(pg_circuit_shared->lock);

	if (bytes_per_sec_out)
		*bytes_per_sec_out = bps;
	if (pressure_level_out)
		*pressure_level_out = level;
}

void
pg_circuit_wal_cache_read(double *bytes_per_sec_out, int *pressure_level_out)
{
	if (bytes_per_sec_out)
		*bytes_per_sec_out = 0;
	if (pressure_level_out)
		*pressure_level_out = 0;
	if (pg_circuit_shared == NULL)
		return;

	LWLockAcquire(pg_circuit_shared->lock, LW_SHARED);
	if (pg_circuit_shared->wal.valid)
	{
		if (bytes_per_sec_out)
			*bytes_per_sec_out = pg_circuit_shared->wal.bytes_per_sec;
		if (pressure_level_out)
			*pressure_level_out = pg_circuit_shared->wal.pressure_level;
	}
	LWLockRelease(pg_circuit_shared->lock);
}
