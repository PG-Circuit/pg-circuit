/*-------------------------------------------------------------------------
 *
 * sql/pg_circuit--0.1.0.sql
 *		SQL objects for PG Circuit Community 0.1.0
 *
 *-------------------------------------------------------------------------
 */

CREATE FUNCTION pg_circuit_version()
RETURNS text
LANGUAGE C STRICT PARALLEL SAFE
AS 'MODULE_PATHNAME', 'pg_circuit_version';

CREATE FUNCTION pg_circuit_status(
    OUT extension_version text,
    OUT postgres_version text,
    OUT enabled boolean,
    OUT mode text,
    OUT configured_runtime_mode text,
    OUT effective_runtime_mode text,
    OUT risk_warn_threshold integer,
    OUT risk_block_threshold integer
)
RETURNS record
LANGUAGE C STRICT PARALLEL SAFE
AS 'MODULE_PATHNAME', 'pg_circuit_status';

CREATE FUNCTION pg_circuit_runtime_state(
    OUT runtime_mode text,
    OUT pressure_score integer,
    OUT active_transactions integer,
    OUT long_transactions integer,
    OUT idle_in_transaction integer,
    OUT blocked_sessions integer,
    OUT blocking_sessions integer,
    OUT max_replication_lag_seconds double precision,
    OUT wal_pressure text,
    OUT wal_bytes_per_sec double precision,
    OUT active_connections integer,
    OUT connection_pressure_score integer,
    OUT oldest_transaction_age_seconds double precision,
    OUT max_lock_chain_depth integer,
    OUT max_lock_descendants integer,
    OUT replica_count integer,
    OUT replication_health text,
    OUT max_write_lag_seconds double precision,
    OUT max_flush_lag_seconds double precision,
    OUT max_replay_lag_seconds double precision,
    OUT pressure_tx integer,
    OUT pressure_explain text
)
RETURNS record
LANGUAGE C STRICT PARALLEL SAFE
AS 'MODULE_PATHNAME', 'pg_circuit_runtime_state';

CREATE FUNCTION pg_circuit_explain_risk(
    operation text,
    estimated_rows double precision,
    relation_size_bytes bigint,
    runtime_mode text,
    replication_lag_seconds double precision,
    OUT score integer,
    OUT decision text,
    OUT risk_level text,
    OUT findings text
)
RETURNS record
LANGUAGE C STRICT PARALLEL SAFE
AS 'MODULE_PATHNAME', 'pg_circuit_explain_risk';

CREATE FUNCTION pg_circuit_blockers(
    OUT blocked_pid integer,
    OUT blocking_pid integer,
    OUT blocked_for double precision,
    OUT blocking_transaction_age double precision,
    OUT database_name text,
    OUT relation_name text,
    OUT lock_type text
)
RETURNS SETOF record
LANGUAGE C STRICT PARALLEL SAFE
AS 'MODULE_PATHNAME', 'pg_circuit_blockers';

CREATE FUNCTION pg_circuit_lock_summary(
    OUT blocked_sessions integer,
    OUT blocking_sessions integer,
    OUT max_blocked_for_seconds double precision,
    OUT has_cycle boolean,
    OUT max_chain_depth integer,
    OUT max_descendant_count integer
)
RETURNS record
LANGUAGE C STRICT PARALLEL SAFE
AS 'MODULE_PATHNAME', 'pg_circuit_lock_summary';

CREATE FUNCTION pg_circuit_explain_risk_ex(
    operation text,
    estimated_rows double precision,
    relation_size_bytes bigint,
    runtime_mode text,
    replication_lag_seconds double precision,
    blocked_sessions integer,
    blocking_sessions integer,
    max_lock_wait_seconds double precision,
    OUT score integer,
    OUT decision text,
    OUT risk_level text,
    OUT findings text
)
RETURNS record
LANGUAGE C STRICT PARALLEL SAFE
AS 'MODULE_PATHNAME', 'pg_circuit_explain_risk_ex';

CREATE FUNCTION pg_circuit_events(
    OUT event_time timestamptz,
    OUT pid integer,
    OUT database_name text,
    OUT user_name text,
    OUT operation_type text,
    OUT relation_name text,
    OUT risk_score integer,
    OUT risk_level text,
    OUT runtime_mode text,
    OUT decision text,
    OUT rule_ids text,
    OUT event_id bigint,
    OUT decision_id bigint,
    OUT fingerprint text,
    OUT base_decision text
)
RETURNS SETOF record
LANGUAGE C STRICT PARALLEL SAFE
AS 'MODULE_PATHNAME', 'pg_circuit_events';

REVOKE ALL ON FUNCTION pg_circuit_version() FROM PUBLIC;
REVOKE ALL ON FUNCTION pg_circuit_status() FROM PUBLIC;
REVOKE ALL ON FUNCTION pg_circuit_runtime_state() FROM PUBLIC;
REVOKE ALL ON FUNCTION pg_circuit_explain_risk(text, double precision, bigint, text, double precision) FROM PUBLIC;
REVOKE ALL ON FUNCTION pg_circuit_blockers() FROM PUBLIC;
REVOKE ALL ON FUNCTION pg_circuit_lock_summary() FROM PUBLIC;
REVOKE ALL ON FUNCTION pg_circuit_explain_risk_ex(text, double precision, bigint, text, double precision, integer, integer, double precision) FROM PUBLIC;
REVOKE ALL ON FUNCTION pg_circuit_events() FROM PUBLIC;

GRANT EXECUTE ON FUNCTION pg_circuit_version() TO PUBLIC;
GRANT EXECUTE ON FUNCTION pg_circuit_status() TO PUBLIC;
GRANT EXECUTE ON FUNCTION pg_circuit_runtime_state() TO PUBLIC;
GRANT EXECUTE ON FUNCTION pg_circuit_explain_risk(text, double precision, bigint, text, double precision) TO PUBLIC;
GRANT EXECUTE ON FUNCTION pg_circuit_blockers() TO PUBLIC;
GRANT EXECUTE ON FUNCTION pg_circuit_lock_summary() TO PUBLIC;
GRANT EXECUTE ON FUNCTION pg_circuit_explain_risk_ex(text, double precision, bigint, text, double precision, integer, integer, double precision) TO PUBLIC;
GRANT EXECUTE ON FUNCTION pg_circuit_events() TO PUBLIC;
