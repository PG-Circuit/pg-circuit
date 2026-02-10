-- runtime: status + NORMAL-only safety mode (Community)
CREATE EXTENSION IF NOT EXISTS pg_circuit;

SELECT runtime_mode IS NOT NULL AS has_runtime_mode,
       pressure_score >= 0 AS has_pressure,
       active_transactions >= 0 AS has_active_tx
FROM pg_circuit_runtime_state();

SELECT configured_runtime_mode, effective_runtime_mode
FROM pg_circuit_status();

-- Community always resolves to NORMAL regardless of configured runtime_mode
SET pg_circuit.runtime_mode = normal;
SELECT runtime_mode FROM pg_circuit_runtime_state();

SET pg_circuit.runtime_mode = protect;
SELECT runtime_mode FROM pg_circuit_runtime_state();

SET pg_circuit.runtime_mode = emergency;
SELECT runtime_mode FROM pg_circuit_runtime_state();

SET pg_circuit.runtime_mode = auto;
SELECT runtime_mode FROM pg_circuit_runtime_state();

-- WHERE-qualified DML is not blocked in Community
DROP TABLE IF EXISTS runtime_write;
CREATE TABLE runtime_write AS SELECT generate_series(1, 1000) AS id;
ANALYZE runtime_write;
SET pg_circuit.mode = enforce;
UPDATE runtime_write SET id = id WHERE id > 0;
SELECT count(*) FROM runtime_write;

SET pg_circuit.mode = observe;
DROP TABLE runtime_write;
