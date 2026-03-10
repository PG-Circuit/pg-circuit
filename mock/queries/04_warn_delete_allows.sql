-- Same DELETE in warn: WARNING + rows deleted, then restore
SET pg_circuit.mode = warn;
SET pg_circuit.runtime_mode = normal;

SELECT count(*) AS before_count FROM shop.order_items;
DELETE FROM shop.order_items;
SELECT count(*) AS after_delete FROM shop.order_items;

-- restore items for later queries
INSERT INTO shop.order_items (id, order_id, product_id, qty, unit_cents)
SELECT g,
       1 + ((g - 1) % 200),
       1 + ((g - 1) % 40),
       1 + (g % 3),
       500 + ((g % 40) * 37)
FROM generate_series(1, 500) g;
ANALYZE shop.order_items;
SELECT count(*) AS restored FROM shop.order_items;
