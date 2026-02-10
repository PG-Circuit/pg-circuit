-- events: WARN/BLOCK decisions land in bounded shared-memory history
CREATE EXTENSION IF NOT EXISTS pg_circuit;

CREATE TABLE events_demo (id int);
INSERT INTO events_demo SELECT generate_series(1, 5);
ANALYZE events_demo;

SET pg_circuit.mode = warn;
-- no-WHERE DELETE should WARN and be recorded
DELETE FROM events_demo;

SELECT count(*) >= 1 AS has_event
FROM pg_circuit_events();

SELECT decision, operation_type, risk_score > 0 AS scored,
       rule_ids LIKE '%PGC001%' AS has_pgc001
FROM pg_circuit_events()
LIMIT 1;

-- ALLOW decisions are not stored
SET pg_circuit.mode = observe;
UPDATE events_demo SET id = id WHERE id = 1;
SELECT count(*) FILTER (WHERE decision = 'ALLOW') = 0 AS no_allow_events
FROM pg_circuit_events();

DROP TABLE events_demo;
