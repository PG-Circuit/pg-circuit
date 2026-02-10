-- prepared: PREPARE/EXECUTE still go through risk analysis
CREATE EXTENSION IF NOT EXISTS pg_circuit;

CREATE TABLE prep_demo (id int);
INSERT INTO prep_demo VALUES (1), (2), (3);
ANALYZE prep_demo;

SET pg_circuit.mode = enforce;

PREPARE dangerous_del AS DELETE FROM prep_demo;
EXECUTE dangerous_del;
SELECT count(*) FROM prep_demo;

PREPARE safe_del AS DELETE FROM prep_demo WHERE id = $1;
EXECUTE safe_del(1);
SELECT count(*) FROM prep_demo;

DEALLOCATE ALL;
SET pg_circuit.mode = observe;
DROP TABLE prep_demo;
