-- Snapshot of mock data
SELECT 'customers' AS relation, count(*) FROM shop.customers
UNION ALL SELECT 'products', count(*) FROM shop.products
UNION ALL SELECT 'orders', count(*) FROM shop.orders
UNION ALL SELECT 'order_items', count(*) FROM shop.order_items
UNION ALL SELECT 'secrets', count(*) FROM circuit_protected.secrets
ORDER BY 1;

SELECT status, count(*) AS n, sum(total_cents) AS total_cents
FROM shop.orders
GROUP BY status
ORDER BY status;

SELECT country, count(*) AS customers
FROM shop.customers
GROUP BY country
ORDER BY customers DESC;
