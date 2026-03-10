-- Unqualified DELETE blocked in enforce
SET pg_circuit.mode = enforce;
SET pg_circuit.runtime_mode = normal;

SELECT count(*) AS before_count FROM shop.orders;

DELETE FROM shop.orders;

SELECT count(*) AS after_count FROM shop.orders;
