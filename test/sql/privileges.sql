-- privileges: inspection APIs are PUBLIC; fail_closed GUC present
CREATE EXTENSION IF NOT EXISTS pg_circuit;

SELECT extension_version, enabled, mode
FROM pg_circuit_status();

SELECT runtime_mode IS NOT NULL AS can_runtime
FROM pg_circuit_runtime_state();

SELECT count(*) >= 0 AS can_events FROM pg_circuit_events();

SHOW pg_circuit.fail_closed;
