SELECT extension_version, mode, configured_runtime_mode, effective_runtime_mode,
       risk_warn_threshold, risk_block_threshold
FROM pg_circuit_status();

SELECT runtime_mode, pressure_score, long_transactions, blocked_sessions,
       wal_pressure, wal_bytes_per_sec
FROM pg_circuit_runtime_state();
