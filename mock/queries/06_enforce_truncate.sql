-- TRUNCATE blocked
SET pg_circuit.mode = enforce;
SET pg_circuit.runtime_mode = normal;

SELECT count(*) AS products_before FROM shop.products;
TRUNCATE shop.products CASCADE;
SELECT count(*) AS products_after FROM shop.products;
