/*-------------------------------------------------------------------------
 *
 * shmem.h
 *		Shared-memory event ring and WAL cache (Community)
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_CIRCUIT_SHMEM_H
#define PG_CIRCUIT_SHMEM_H

#include "pg_circuit.h"
#include "event.h"
#include "storage/lwlock.h"
#include "utils/timestamp.h"

#define PGC_RULE_IDS_MAX			64

typedef struct PgCircuitSharedEvent
{
	TimestampTz timestamp;
	int64		event_id;
	int64		decision_id;
	int			backend_pid;
	Oid			database_oid;
	Oid			user_oid;
	Oid			relation_oid;
	int16		decision;
	int16		base_decision;
	int16		runtime_mode;
	int16		operation;
	int			risk_score;
	char		primary_rule[16];
	char		rule_ids[PGC_RULE_IDS_MAX];
	char		relation_name[NAMEDATALEN];
	char		database_name[NAMEDATALEN];
	char		user_name[NAMEDATALEN];
	char		fingerprint[33];
} PgCircuitSharedEvent;

typedef struct PgCircuitWalCache
{
	bool		valid;
	uint64		wal_bytes;
	TimestampTz sample_time;
	double		bytes_per_sec;
	int			pressure_level;
} PgCircuitWalCache;

typedef struct PgCircuitSharedState
{
	uint32		magic;
	uint32		version;
	LWLock	   *lock;
	int			capacity;
	int			head;
	int			count;
	PgCircuitWalCache wal;
	int64		next_event_id;
	int64		next_decision_id;
	PgCircuitSharedEvent events[FLEXIBLE_ARRAY_MEMBER];
} PgCircuitSharedState;

extern PgCircuitSharedState *pg_circuit_shared;

extern Size pg_circuit_shared_memsize(void);
extern void pg_circuit_shmem_register(void);
extern void pg_circuit_event_record(const PgCircuitRiskEvent *event,
									const char *rule_ids,
									const char *relation_name,
									const char *database_name,
									const char *user_name,
									const PgCircuitPolicyResult *policy);
extern int	pg_circuit_event_snapshot(PgCircuitSharedEvent *out, int max_out);
extern void pg_circuit_wal_sample_update(uint64 wal_bytes, TimestampTz now,
										 double *bytes_per_sec_out,
										 int *pressure_level_out);
extern void pg_circuit_wal_cache_read(double *bytes_per_sec_out,
									 int *pressure_level_out);

#endif							/* PG_CIRCUIT_SHMEM_H */
