-- blockers: blocker graph API (deterministic; live multi-session in docker-test)
CREATE EXTENSION IF NOT EXISTS pg_circuit;

SELECT blocked_sessions, blocking_sessions, max_blocked_for_seconds >= 0 AS has_max_wait,
       has_cycle, max_chain_depth >= 0 AS has_depth, max_descendant_count >= 0 AS has_desc
FROM pg_circuit_lock_summary();

SELECT count(*) >= 0 AS blockers_callable FROM pg_circuit_blockers();

-- Synthetic lock-pressure scoring remains deterministic
SELECT score > 0 AS has_score
FROM pg_circuit_explain_risk_ex(
  'alter_table', 0, (5::bigint * 1024 * 1024 * 1024), 'protect', 0, 5, 1, 20);
