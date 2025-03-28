/*-------------------------------------------------------------------------
 *
 * relations.h
 *		Relation metadata helpers
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_CIRCUIT_RELATIONS_H
#define PG_CIRCUIT_RELATIONS_H

#include "postgres.h"

#define PG_CIRCUIT_QUALNAME_BUFLEN (NAMEDATALEN * 2 + 2)

extern void pg_circuit_relation_name(Oid relid, char *buf, size_t buflen);
extern void pg_circuit_relation_qualname(Oid relid, char *buf, size_t buflen);
extern double pg_circuit_relation_estimated_tuples(Oid relid);
extern double pg_circuit_relation_physical_tuple_bound(Oid relid);
extern double pg_circuit_relation_table_rows_estimate(Oid relid);
extern int64 pg_circuit_relation_size(Oid relid);

#endif							/* PG_CIRCUIT_RELATIONS_H */
