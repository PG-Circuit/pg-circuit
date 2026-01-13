-- basic: extension loads and status API works
CREATE EXTENSION IF NOT EXISTS pg_circuit;

SELECT pg_circuit_version();

SELECT enabled,
       extension_version,
       mode,
       configured_runtime_mode,
       risk_warn_threshold,
       risk_block_threshold
FROM pg_circuit_status();

SHOW pg_circuit.enabled;
SHOW pg_circuit.mode;
SHOW pg_circuit.runtime_mode;
SHOW pg_circuit.debug;
