-- destructive: PGC001-PGC005 detection (Community)
CREATE EXTENSION IF NOT EXISTS pg_circuit;

DROP TABLE IF EXISTS dest_users;
CREATE TABLE dest_users (id int primary key, active bool);
INSERT INTO dest_users VALUES (1, true), (2, true), (3, false);
ANALYZE dest_users;

SET pg_circuit.mode = enforce;

-- PGC001 DELETE without WHERE => blocked
DELETE FROM dest_users;

-- DELETE with WHERE => allowed
DELETE FROM dest_users WHERE id = 1;
SELECT count(*) FROM dest_users;

-- PGC002 UPDATE without WHERE => blocked
UPDATE dest_users SET active = false;

-- UPDATE with WHERE => allowed
UPDATE dest_users SET active = false WHERE id = 2;
SELECT active FROM dest_users ORDER BY id;

-- PGC003 TRUNCATE => blocked
TRUNCATE dest_users;

-- PGC004 DROP TABLE => blocked
DROP TABLE dest_users;

-- WHERE-qualified DML stays quiet in Community (no blast-radius rules)
CREATE TABLE tiny_demo AS SELECT g AS id FROM generate_series(1, 5) g;
ANALYZE tiny_demo;
DELETE FROM tiny_demo WHERE id <= 3;
SELECT count(*) AS tiny_left FROM tiny_demo;

CREATE TABLE cov_demo AS
  SELECT g AS id, false AS archived FROM generate_series(1, 20000) g;
ANALYZE cov_demo;
UPDATE cov_demo SET archived = true WHERE id BETWEEN 1 AND 20000;
SELECT count(*) FILTER (WHERE archived) AS archived_ok FROM cov_demo;

-- cleanup
SET pg_circuit.mode = observe;
DROP TABLE dest_users;
DROP TABLE tiny_demo;
DROP TABLE cov_demo;
