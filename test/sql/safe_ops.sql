-- safe_ops: ordinary traffic must remain unaffected
CREATE EXTENSION IF NOT EXISTS pg_circuit;

SET pg_circuit.mode = enforce;

CREATE TABLE safe_ops (id int, n int);
INSERT INTO safe_ops VALUES (1, 10);
INSERT INTO safe_ops VALUES (2, 20);

SELECT id, n FROM safe_ops ORDER BY id;

UPDATE safe_ops SET n = n + 1 WHERE id = 1;
SELECT n FROM safe_ops WHERE id = 1;

DELETE FROM safe_ops WHERE id = 2;
SELECT count(*) FROM safe_ops;

-- cleanup
SET pg_circuit.mode = observe;
DROP TABLE safe_ops;
