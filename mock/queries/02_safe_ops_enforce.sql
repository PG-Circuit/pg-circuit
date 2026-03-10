-- Safe reads under enforce — must succeed
SET pg_circuit.mode = enforce;
SET pg_circuit.runtime_mode = normal;

SELECT count(*) AS paid_orders
FROM shop.orders
WHERE status = 'paid';

SELECT p.sku, p.name, sum(oi.qty) AS units
FROM shop.order_items oi
JOIN shop.products p ON p.id = oi.product_id
GROUP BY p.sku, p.name
ORDER BY units DESC
LIMIT 5;

UPDATE shop.orders
SET status = status
WHERE id = 1
RETURNING id, status;
