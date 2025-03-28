/*-------------------------------------------------------------------------
 *
 * hooks.h
 *		PostgreSQL hook registration for PG Circuit
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_CIRCUIT_HOOKS_H
#define PG_CIRCUIT_HOOKS_H

#include "pg_circuit.h"

extern void pg_circuit_install_hooks(void);
extern void pg_circuit_uninstall_hooks(void);

#endif							/* PG_CIRCUIT_HOOKS_H */
