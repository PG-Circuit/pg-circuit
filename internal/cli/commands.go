package cli

import (
	"context"
	"encoding/json"
	"fmt"
	"os"
	"strings"
)

type StatusRow struct {
	ExtensionVersion      string `json:"extension_version"`
	PostgresVersion       string `json:"postgres_version"`
	Enabled               bool   `json:"enabled"`
	Mode                  string `json:"mode"`
	ConfiguredRuntimeMode string `json:"configured_runtime_mode"`
	EffectiveRuntimeMode  string `json:"effective_runtime_mode"`
	RiskWarnThreshold     int32  `json:"risk_warn_threshold"`
	RiskBlockThreshold    int32  `json:"risk_block_threshold"`
	ServerVersion         string `json:"server_version,omitempty"`
}

type RuntimeRow struct {
	RuntimeMode              string  `json:"runtime_mode"`
	PressureScore            int32   `json:"pressure_score"`
	ActiveTransactions       int32   `json:"active_transactions"`
	LongTransactions         int32   `json:"long_transactions"`
	IdleInTransaction        int32   `json:"idle_in_transaction"`
	BlockedSessions          int32   `json:"blocked_sessions"`
	BlockingSessions         int32   `json:"blocking_sessions"`
	MaxReplicationLagSeconds float64 `json:"max_replication_lag_seconds"`
	WalPressure              string  `json:"wal_pressure"`
	WalBytesPerSec           float64 `json:"wal_bytes_per_sec"`
}

type BlockerRow struct {
	BlockedPID             int32   `json:"blocked_pid"`
	BlockingPID            int32   `json:"blocking_pid"`
	BlockedFor             float64 `json:"blocked_for"`
	BlockingTransactionAge float64 `json:"blocking_transaction_age"`
	DatabaseName           *string `json:"database_name"`
	RelationName           *string `json:"relation_name"`
	LockType               string  `json:"lock_type"`
}

type EventRow struct {
	EventTime     any     `json:"event_time"`
	PID           int32   `json:"pid"`
	DatabaseName  *string `json:"database_name"`
	UserName      *string `json:"user_name"`
	OperationType string  `json:"operation_type"`
	RelationName  *string `json:"relation_name"`
	RiskScore     int32   `json:"risk_score"`
	RiskLevel     string  `json:"risk_level"`
	RuntimeMode   string  `json:"runtime_mode"`
	Decision      string  `json:"decision"`
	RuleIDs       *string `json:"rule_ids"`
}

type DoctorReport struct {
	OK                 bool     `json:"ok"`
	Connected          bool     `json:"connected"`
	ServerVersion      string   `json:"server_version"`
	SupportedServer    bool     `json:"supported_server"`
	ExtensionInstalled bool     `json:"extension_installed"`
	ExtensionVersion   string   `json:"extension_version,omitempty"`
	CanInspect         bool     `json:"can_inspect"`
	Checks             []string `json:"checks"`
	Warnings           []string `json:"warnings"`
}

func printJSON(v any) int {
	enc := json.NewEncoder(os.Stdout)
	enc.SetIndent("", "  ")
	if err := enc.Encode(v); err != nil {
		fmt.Fprintf(os.Stderr, "error: %v\n", err)
		return 1
	}
	return 0
}

func CmdStatus(ctx context.Context, c *Client, cfg Config) int {
	row, err := fetchStatus(ctx, c)
	if err != nil {
		fmt.Fprintf(os.Stderr, "error: %v\n", err)
		return 1
	}
	if cfg.Format == "json" {
		return printJSON(row)
	}
	fmt.Println("PG Circuit")
	fmt.Println()
	fmt.Printf("PostgreSQL:       %s\n", displayPGVersion(row))
	fmt.Printf("Extension:        %s\n", row.ExtensionVersion)
	fmt.Printf("Mode:             %s\n", row.Mode)
	fmt.Printf("Runtime mode:     %s\n", strings.ToUpper(row.EffectiveRuntimeMode))
	fmt.Printf("Configured:       %s\n", row.ConfiguredRuntimeMode)
	fmt.Printf("Enabled:          %v\n", row.Enabled)
	fmt.Printf("Warn threshold:   %d\n", row.RiskWarnThreshold)
	fmt.Printf("Block threshold:  %d\n", row.RiskBlockThreshold)
	fmt.Println()
	fmt.Println("Note: Community effective runtime mode is always NORMAL;")
	fmt.Println("      pressure remains visible via `pgcircuit runtime`.")
	return 0
}

func displayPGVersion(row StatusRow) string {
	if row.ServerVersion != "" {
		return row.ServerVersion
	}
	return row.PostgresVersion
}

func fetchStatus(ctx context.Context, c *Client) (StatusRow, error) {
	var row StatusRow
	err := c.Conn.QueryRow(ctx, `
		SELECT extension_version, postgres_version, enabled, mode,
		       configured_runtime_mode, effective_runtime_mode,
		       risk_warn_threshold, risk_block_threshold
		FROM pg_circuit_status()`).Scan(
		&row.ExtensionVersion, &row.PostgresVersion, &row.Enabled, &row.Mode,
		&row.ConfiguredRuntimeMode, &row.EffectiveRuntimeMode,
		&row.RiskWarnThreshold, &row.RiskBlockThreshold,
	)
	if err != nil {
		return row, err
	}
	_ = c.Conn.QueryRow(ctx, `SHOW server_version`).Scan(&row.ServerVersion)
	return row, nil
}

func CmdRuntime(ctx context.Context, c *Client, cfg Config) int {
	row, err := fetchRuntime(ctx, c)
	if err != nil {
		fmt.Fprintf(os.Stderr, "error: %v\n", err)
		return 1
	}
	if cfg.Format == "json" {
		return printJSON(row)
	}
	fmt.Println("PG Circuit runtime")
	fmt.Println()
	fmt.Printf("Runtime mode:     %s\n", strings.ToUpper(row.RuntimeMode))
	fmt.Printf("Pressure score:   %d/100\n", row.PressureScore)
	fmt.Printf("Replication lag:  %.1fs\n", row.MaxReplicationLagSeconds)
	fmt.Printf("Blocked sessions: %d\n", row.BlockedSessions)
	fmt.Printf("Blocking sessions: %d\n", row.BlockingSessions)
	fmt.Printf("Long transactions: %d\n", row.LongTransactions)
	fmt.Printf("Idle in xact:     %d\n", row.IdleInTransaction)
	fmt.Printf("WAL pressure:     %s\n", row.WalPressure)
	fmt.Printf("WAL bytes/sec:    %.0f\n", row.WalBytesPerSec)
	fmt.Println()
	fmt.Println("Note: Community does not escalate from pressure; score is display-only.")
	return 0
}

func fetchRuntime(ctx context.Context, c *Client) (RuntimeRow, error) {
	var row RuntimeRow
	err := c.Conn.QueryRow(ctx, `
		SELECT runtime_mode, pressure_score, active_transactions, long_transactions,
		       idle_in_transaction, blocked_sessions, blocking_sessions,
		       max_replication_lag_seconds, wal_pressure, wal_bytes_per_sec
		FROM pg_circuit_runtime_state()`).Scan(
		&row.RuntimeMode, &row.PressureScore, &row.ActiveTransactions, &row.LongTransactions,
		&row.IdleInTransaction, &row.BlockedSessions, &row.BlockingSessions,
		&row.MaxReplicationLagSeconds, &row.WalPressure, &row.WalBytesPerSec,
	)
	return row, err
}

func CmdBlockers(ctx context.Context, c *Client, cfg Config) int {
	rows, err := c.Conn.Query(ctx, `
		SELECT blocked_pid, blocking_pid, blocked_for, blocking_transaction_age,
		       database_name, relation_name, lock_type
		FROM pg_circuit_blockers()`)
	if err != nil {
		fmt.Fprintf(os.Stderr, "error: %v\n", err)
		return 1
	}
	defer rows.Close()

	var out []BlockerRow
	for rows.Next() {
		var r BlockerRow
		if err := rows.Scan(&r.BlockedPID, &r.BlockingPID, &r.BlockedFor, &r.BlockingTransactionAge,
			&r.DatabaseName, &r.RelationName, &r.LockType); err != nil {
			fmt.Fprintf(os.Stderr, "error: %v\n", err)
			return 1
		}
		out = append(out, r)
	}
	if err := rows.Err(); err != nil {
		fmt.Fprintf(os.Stderr, "error: %v\n", err)
		return 1
	}
	if cfg.Format == "json" {
		if out == nil {
			out = []BlockerRow{}
		}
		return printJSON(out)
	}
	if len(out) == 0 {
		fmt.Println("No lock blockers.")
		return 0
	}
	for _, r := range out {
		rel := "-"
		if r.RelationName != nil {
			rel = *r.RelationName
		}
		fmt.Printf("blocked=%d blocking=%d wait=%.1fs rel=%s lock=%s\n",
			r.BlockedPID, r.BlockingPID, r.BlockedFor, rel, r.LockType)
	}
	return 0
}

func CmdEvents(ctx context.Context, c *Client, cfg Config, _ []string) int {
	rows, err := c.Conn.Query(ctx, `
		SELECT event_time, pid, database_name, user_name, operation_type,
		       relation_name, risk_score, risk_level, runtime_mode, decision, rule_ids
		FROM pg_circuit_events()`)
	if err != nil {
		fmt.Fprintf(os.Stderr, "error: %v\n", err)
		return 1
	}
	defer rows.Close()

	var out []EventRow
	for rows.Next() {
		var r EventRow
		if err := rows.Scan(&r.EventTime, &r.PID, &r.DatabaseName, &r.UserName, &r.OperationType,
			&r.RelationName, &r.RiskScore, &r.RiskLevel, &r.RuntimeMode, &r.Decision, &r.RuleIDs); err != nil {
			fmt.Fprintf(os.Stderr, "error: %v\n", err)
			return 1
		}
		out = append(out, r)
	}
	if err := rows.Err(); err != nil {
		fmt.Fprintf(os.Stderr, "error: %v\n", err)
		return 1
	}
	if cfg.Format == "json" {
		if out == nil {
			out = []EventRow{}
		}
		return printJSON(out)
	}
	if len(out) == 0 {
		fmt.Println("No recent WARN/BLOCK events.")
		return 0
	}
	for _, r := range out {
		rules := ""
		if r.RuleIDs != nil {
			rules = *r.RuleIDs
		}
		fmt.Printf("%v pid=%d %s score=%d mode=%s decision=%s rules=%s\n",
			r.EventTime, r.PID, r.OperationType, r.RiskScore, r.RuntimeMode, r.Decision, rules)
	}
	return 0
}

func CmdDoctor(ctx context.Context, c *Client, cfg Config) int {
	report := DoctorReport{Connected: true, Checks: []string{}, Warnings: []string{}}

	if err := c.Conn.QueryRow(ctx, `SHOW server_version`).Scan(&report.ServerVersion); err != nil {
		report.Connected = false
		report.Warnings = append(report.Warnings, fmt.Sprintf("server_version: %v", err))
	} else {
		report.Checks = append(report.Checks, "connected to PostgreSQL")
		report.SupportedServer = supportedServer(report.ServerVersion)
		if report.SupportedServer {
			report.Checks = append(report.Checks, "server version supported (16–18)")
		} else {
			report.Warnings = append(report.Warnings, "server version may be outside supported 16–18 range")
		}
	}

	var extVer string
	err := c.Conn.QueryRow(ctx, `
		SELECT extversion FROM pg_extension WHERE extname = 'pg_circuit'`).Scan(&extVer)
	if err != nil {
		report.ExtensionInstalled = false
		report.Warnings = append(report.Warnings, "extension pg_circuit is not installed")
	} else {
		report.ExtensionInstalled = true
		report.ExtensionVersion = extVer
		report.Checks = append(report.Checks, "extension installed ("+extVer+")")
	}

	if report.ExtensionInstalled {
		status, err := fetchStatus(ctx, c)
		if err != nil {
			report.CanInspect = false
			report.Warnings = append(report.Warnings, "pg_circuit_status() failed: "+err.Error())
		} else {
			report.CanInspect = true
			report.Checks = append(report.Checks, "SQL inspection APIs callable")
			if !strings.EqualFold(status.ConfiguredRuntimeMode, "normal") {
				report.Warnings = append(report.Warnings,
					"configured runtime_mode="+status.ConfiguredRuntimeMode+" is ignored in Community (effective mode stays NORMAL)")
			}
		}
		_, err = fetchRuntime(ctx, c)
		if err != nil {
			report.Warnings = append(report.Warnings, "pg_circuit_runtime_state() failed: "+err.Error())
		}
	}

	report.OK = report.Connected && report.ExtensionInstalled && report.CanInspect && len(report.Warnings) == 0

	if cfg.Format == "json" {
		return printJSON(report)
	}

	fmt.Println("PG Circuit doctor")
	fmt.Println()
	fmt.Printf("Connection:       %s\n", SanitizeDSN(cfg.DSN))
	fmt.Printf("PostgreSQL:       %s\n", report.ServerVersion)
	fmt.Printf("Extension:        %v (%s)\n", report.ExtensionInstalled, report.ExtensionVersion)
	fmt.Printf("Can inspect:      %v\n", report.CanInspect)
	fmt.Println()
	for _, c := range report.Checks {
		fmt.Printf("  ok  %s\n", c)
	}
	for _, w := range report.Warnings {
		fmt.Printf("  !!  %s\n", w)
	}
	if report.OK {
		fmt.Println("\nOverall: healthy")
		fmt.Println()
		fmt.Println("Operator loop when something is blocked:")
		fmt.Println("  pgcircuit status → runtime → events")
		fmt.Println("  SQL: SELECT * FROM pg_circuit_explain_risk('delete_no_where', 0, 0, 'normal', 0);")
		return 0
	}
	fmt.Println("\nOverall: issues found")
	return 1
}

func supportedServer(ver string) bool {
	ver = strings.TrimSpace(ver)
	return strings.HasPrefix(ver, "16.") || strings.HasPrefix(ver, "17.") ||
		strings.HasPrefix(ver, "18.") || ver == "16" || ver == "17" || ver == "18"
}
