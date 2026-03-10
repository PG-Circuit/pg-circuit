-- UPDATE without WHERE blocked
SET pg_circuit.mode = enforce;
SET pg_circuit.runtime_mode = normal;

SELECT count(*) FILTER (WHERE status = 'pending') AS pending_before
FROM shop.orders;

UPDATE shop.orders SET status = 'cancelled';

SELECT count(*) FILTER (WHERE status = 'pending') AS pending_after
FROM shop.orders;
