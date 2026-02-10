-- ddl: ALTER / INDEX / maintenance protection (Community)
CREATE EXTENSION IF NOT EXISTS pg_circuit;

DROP TABLE IF EXISTS ddl_demo;
CREATE TABLE ddl_demo (id int primary key, n int, t text);
INSERT INTO ddl_demo SELECT g, g, 'x' FROM generate_series(1, 100) g;
ANALYZE ddl_demo;

-- Ordinary ANALYZE must remain unaffected
ANALYZE ddl_demo;

-- Live CREATE INDEX under observe (allowed) then under enforce
SET pg_circuit.mode = observe;
CREATE INDEX ddl_demo_n_idx ON ddl_demo (n);
DROP INDEX ddl_demo_n_idx;

SET pg_circuit.mode = enforce;
SET pg_circuit.runtime_mode = normal;
CREATE INDEX ddl_demo_n_idx ON ddl_demo (n);

SET pg_circuit.mode = observe;
DROP INDEX IF EXISTS ddl_demo_n_idx;

-- Synthetic DDL scoring
SET pg_circuit.mode = enforce;

SELECT score, decision
FROM pg_circuit_explain_risk('alter_table', 0, (2::bigint * 1024 * 1024 * 1024), 'normal', 0);

SELECT score, decision
FROM pg_circuit_explain_risk('create_index', 0, (2::bigint * 1024 * 1024 * 1024), 'normal', 0);

SELECT score, decision
FROM pg_circuit_explain_risk('create_index_concurrently', 0, (2::bigint * 1024 * 1024 * 1024), 'normal', 0);

SELECT score, decision
FROM pg_circuit_explain_risk('vacuum_full', 0, (2::bigint * 1024 * 1024 * 1024), 'normal', 0);

SELECT score, decision
FROM pg_circuit_explain_risk('cluster', 0, (2::bigint * 1024 * 1024 * 1024), 'normal', 0);

SELECT score, decision
FROM pg_circuit_explain_risk('reindex', 0, (2::bigint * 1024 * 1024 * 1024), 'normal', 0);

-- Live ALTER TYPE under enforce (lower block threshold for PGC012 base 60)
SET pg_circuit.risk_block_threshold = 60;
SET pg_circuit.mode = enforce;
ALTER TABLE ddl_demo ALTER COLUMN n TYPE bigint;

-- cleanup
SET pg_circuit.mode = observe;
SET pg_circuit.risk_block_threshold = 80;
DROP TABLE ddl_demo;
