-- modes: observe / warn / enforce behavior
CREATE EXTENSION IF NOT EXISTS pg_circuit;

DROP TABLE IF EXISTS mode_demo;
CREATE TABLE mode_demo (id int);
INSERT INTO mode_demo VALUES (1), (2), (3);
ANALYZE mode_demo;

-- observe: never warns/blocks
SET pg_circuit.mode = observe;
DELETE FROM mode_demo;
INSERT INTO mode_demo VALUES (1), (2), (3);
ANALYZE mode_demo;

-- warn: emits warning, does not block
SET pg_circuit.mode = warn;
DELETE FROM mode_demo;
SELECT count(*) FROM mode_demo;
INSERT INTO mode_demo VALUES (1), (2), (3);
ANALYZE mode_demo;

-- enforce: blocks
SET pg_circuit.mode = enforce;
DELETE FROM mode_demo;
SELECT count(*) FROM mode_demo;

-- cleanup under observe
SET pg_circuit.mode = observe;
DROP TABLE mode_demo;
