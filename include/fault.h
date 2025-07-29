/*-------------------------------------------------------------------------
 *
 * fault.h
 *		Developer-only fault injection (compile with -DPGCIRCUIT_FAULT_INJECTION)
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_CIRCUIT_FAULT_H
#define PG_CIRCUIT_FAULT_H

#include "pg_circuit.h"

#ifdef PGCIRCUIT_FAULT_INJECTION
extern char *pg_circuit_fault;

static inline bool
pg_circuit_fault_is(const char *name)
{
	return (pg_circuit_fault != NULL &&
			pg_circuit_fault[0] != '\0' &&
			pg_strcasecmp(pg_circuit_fault, name) == 0);
}
#else
#define pg_circuit_fault_is(name) false
#endif

#endif							/* PG_CIRCUIT_FAULT_H */
