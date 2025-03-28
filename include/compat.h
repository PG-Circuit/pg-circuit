/*-------------------------------------------------------------------------
 *
 * compat.h
 *		Minimal PG 16/17/18 compatibility helpers
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_CIRCUIT_COMPAT_H
#define PG_CIRCUIT_COMPAT_H

#include "postgres.h"

/*
 * Keep this file intentionally small. Only add macros when a real API
 * difference across supported majors requires it.
 */

#if PG_VERSION_NUM < 160000 || PG_VERSION_NUM >= 190000
#error "pg_circuit supports PostgreSQL 16, 17, and 18 only"
#endif

#endif							/* PG_CIRCUIT_COMPAT_H */
