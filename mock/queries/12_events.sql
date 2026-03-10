-- Events from earlier blocks/warns
SELECT decision, rule_ids, left(summary, 80) AS summary,
       fingerprint IS NOT NULL AS has_fp
FROM pg_circuit_events()
ORDER BY 1
LIMIT 15;

SELECT count(*) AS event_rows FROM pg_circuit_events();
SELECT count(*) AS blocker_rows FROM pg_circuit_blockers();
