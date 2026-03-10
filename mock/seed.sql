-- mock seed: ecommerce-ish demo dataset for pg_circuit
CREATE EXTENSION IF NOT EXISTS pg_circuit;

CREATE SCHEMA IF NOT EXISTS shop;
DROP TABLE IF EXISTS shop.order_items CASCADE;
DROP TABLE IF EXISTS shop.orders CASCADE;
DROP TABLE IF EXISTS shop.products CASCADE;
DROP TABLE IF EXISTS shop.customers CASCADE;

CREATE TABLE shop.customers (
  id          int PRIMARY KEY,
  email       text NOT NULL,
  country     text NOT NULL,
  created_at  timestamptz NOT NULL DEFAULT now()
);

CREATE TABLE shop.products (
  id          int PRIMARY KEY,
  sku         text NOT NULL UNIQUE,
  name        text NOT NULL,
  price_cents int NOT NULL,
  active      boolean NOT NULL DEFAULT true
);

CREATE TABLE shop.orders (
  id           int PRIMARY KEY,
  customer_id  int NOT NULL REFERENCES shop.customers(id),
  status       text NOT NULL CHECK (status IN ('pending','paid','shipped','cancelled')),
  total_cents  int NOT NULL,
  created_at   timestamptz NOT NULL DEFAULT now()
);

CREATE TABLE shop.order_items (
  id          int PRIMARY KEY,
  order_id    int NOT NULL REFERENCES shop.orders(id),
  product_id  int NOT NULL REFERENCES shop.products(id),
  qty         int NOT NULL,
  unit_cents  int NOT NULL
);

INSERT INTO shop.customers (id, email, country)
SELECT g,
       'user' || g || '@example.com',
       (ARRAY['US','DE','AM','GB','FR'])[1 + (g % 5)]
FROM generate_series(1, 50) g;

INSERT INTO shop.products (id, sku, name, price_cents, active)
SELECT g,
       'SKU-' || lpad(g::text, 4, '0'),
       'Product ' || g,
       500 + (g * 37),
       (g % 7) <> 0
FROM generate_series(1, 40) g;

INSERT INTO shop.orders (id, customer_id, status, total_cents, created_at)
SELECT g,
       1 + ((g - 1) % 50),
       (ARRAY['pending','paid','shipped','cancelled'])[1 + (g % 4)],
       1000 + g * 11,
       now() - ((g % 90) || ' days')::interval
FROM generate_series(1, 200) g;

INSERT INTO shop.order_items (id, order_id, product_id, qty, unit_cents)
SELECT g,
       1 + ((g - 1) % 200),
       1 + ((g - 1) % 40),
       1 + (g % 3),
       500 + ((g % 40) * 37)
FROM generate_series(1, 500) g;

ANALYZE shop.customers;
ANALYZE shop.products;
ANALYZE shop.orders;
ANALYZE shop.order_items;
SELECT 'seed_ok' AS status,
       (SELECT count(*) FROM shop.customers) AS customers,
       (SELECT count(*) FROM shop.products) AS products,
       (SELECT count(*) FROM shop.orders) AS orders,
       (SELECT count(*) FROM shop.order_items) AS order_items
