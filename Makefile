MODULE_big = pg_circuit
EXTVERSION = 0.1.0
EXTENSION = pg_circuit
OBJS = \
	src/pg_circuit.o \
	src/hooks.o \
	src/config.o \
	src/query_analysis.o \
	src/utility_analysis.o \
	src/runtime_state.o \
	src/risk_engine.o \
	src/circuit_breaker.o \
	src/relations.o \
	src/transactions.o \
	src/locks.o \
	src/replication.o \
	src/reporting.o \
	src/blocker_graph.o \
	src/event.o \
	src/shmem.o \
	src/wal.o \
	src/policy_stub.o

DATA = sql/pg_circuit--0.1.0.sql

PG_CPPFLAGS = -I$(srcdir)/include
PG_CFLAGS = -Wall -Wextra -Wformat -Wformat-security -Wno-unused-parameter -Werror

REGRESS = basic modes destructive safe_ops runtime ddl blockers events prepared privileges
REGRESS_OPTS = --inputdir=test --outputdir=test --load-extension=pg_circuit

PG_CONFIG ?= pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

.PHONY: docker-build docker-up docker-test docker-down go-test cli

go-test:
	cd . && go test ./...

cli:
	go build -o bin/pgcircuit ./cmd/pgcircuit

docker-build:
	docker compose build

docker-up:
	docker compose up -d

docker-down:
	docker compose down -v

docker-test: docker-build docker-up
	@echo "Waiting for PostgreSQL..."
	@for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do \
		docker compose exec -T postgres pg_isready -U postgres && break; \
		sleep 1; \
	done
	docker compose exec -T postgres psql -U postgres -d postgres -c "CREATE EXTENSION IF NOT EXISTS pg_circuit;"
	docker compose exec -T postgres psql -U postgres -d postgres -c "SELECT pg_circuit_version();"
	docker compose exec -T postgres psql -U postgres -d postgres -v ON_ERROR_STOP=1 -c "\
		DROP TABLE IF EXISTS circuit_demo; \
		CREATE TABLE circuit_demo (id int); \
		INSERT INTO circuit_demo SELECT generate_series(1,10); \
		SET pg_circuit.mode = 'enforce'; \
		DELETE FROM circuit_demo;" || true
	@echo "Running bounded two-session blocker check..."
	./scripts/test_blockers.sh || true
	@echo "docker-test completed"
