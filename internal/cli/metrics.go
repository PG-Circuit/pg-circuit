package cli

import (
	"context"
	"fmt"
	"io"
	"net/http"
	"os"
	"strings"
	"time"
)

// CmdMetrics exports Prometheus text from live SQL signals.
//
//	pgcircuit metrics                 # one-shot stdout
//	pgcircuit metrics --listen :9187  # scrape endpoint
func CmdMetrics(ctx context.Context, c *Client, cfg Config, args []string) int {
	listen := ""
	for i := 0; i < len(args); i++ {
		a := args[i]
		switch {
		case a == "--listen" && i+1 < len(args):
			i++
			listen = args[i]
		case strings.HasPrefix(a, "--listen="):
			listen = strings.TrimPrefix(a, "--listen=")
		default:
			fmt.Fprintf(os.Stderr, "unknown metrics argument %q\n", a)
			return ExitUsage
		}
	}

	if listen == "" {
		body, err := renderPrometheus(ctx, c)
		if err != nil {
			fmt.Fprintf(os.Stderr, "error: %v\n", err)
			return 1
		}
		fmt.Print(body)
		return 0
	}

	mux := http.NewServeMux()
	mux.HandleFunc("/metrics", func(w http.ResponseWriter, r *http.Request) {
		body, err := renderPrometheus(r.Context(), c)
		if err != nil {
			http.Error(w, err.Error(), http.StatusServiceUnavailable)
			return
		}
		w.Header().Set("Content-Type", "text/plain; version=0.0.4; charset=utf-8")
		_, _ = io.WriteString(w, body)
	})
	mux.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "text/plain; charset=utf-8")
		_, _ = io.WriteString(w, "pgcircuit metrics exporter\nscrapes: GET /metrics\n")
	})

	srv := &http.Server{
		Addr:              listen,
		Handler:           mux,
		ReadHeaderTimeout: 5 * time.Second,
	}
	fmt.Fprintf(os.Stderr, "pgcircuit metrics listening on http://%s/metrics\n", listen)
	if err := srv.ListenAndServe(); err != nil && err != http.ErrServerClosed {
		fmt.Fprintf(os.Stderr, "error: %v\n", err)
		return 1
	}
	return 0
}

func renderPrometheus(ctx context.Context, c *Client) (string, error) {
	status, err := fetchStatus(ctx, c)
	if err != nil {
		return "", err
	}
	runtime, err := fetchRuntime(ctx, c)
	if err != nil {
		return "", err
	}
	events, err := fetchEvents(ctx, c)
	if err != nil {
		return "", err
	}

	var warnN, blockN int
	for _, e := range events {
		switch strings.ToUpper(e.Decision) {
		case "WARN":
			warnN++
		case "BLOCK":
			blockN++
		}
	}

	var b strings.Builder
	writePromHelp(&b, "pgcircuit_up", "gauge", "1 if the exporter reached PostgreSQL")
	fmt.Fprintf(&b, "pgcircuit_up 1\n")

	writePromHelp(&b, "pgcircuit_pressure_score", "gauge", "Combined runtime pressure score 0-100 (display-only in Community)")
	fmt.Fprintf(&b, "pgcircuit_pressure_score %d\n", runtime.PressureScore)

	writePromHelp(&b, "pgcircuit_wal_bytes_per_sec", "gauge", "Sampled WAL byte rate")
	fmt.Fprintf(&b, "pgcircuit_wal_bytes_per_sec %.0f\n", runtime.WalBytesPerSec)

	writePromHelp(&b, "pgcircuit_blocked_sessions", "gauge", "Sessions waiting on locks")
	fmt.Fprintf(&b, "pgcircuit_blocked_sessions %d\n", runtime.BlockedSessions)

	writePromHelp(&b, "pgcircuit_blocking_sessions", "gauge", "Distinct blocking sessions")
	fmt.Fprintf(&b, "pgcircuit_blocking_sessions %d\n", runtime.BlockingSessions)

	writePromHelp(&b, "pgcircuit_long_transactions", "gauge", "Transactions older than long_transaction_seconds")
	fmt.Fprintf(&b, "pgcircuit_long_transactions %d\n", runtime.LongTransactions)

	writePromHelp(&b, "pgcircuit_idle_in_transaction", "gauge", "Idle-in-transaction backends")
	fmt.Fprintf(&b, "pgcircuit_idle_in_transaction %d\n", runtime.IdleInTransaction)

	writePromHelp(&b, "pgcircuit_replication_lag_seconds", "gauge", "Max replication lag seconds")
	fmt.Fprintf(&b, "pgcircuit_replication_lag_seconds %.3f\n", runtime.MaxReplicationLagSeconds)

	writePromHelp(&b, "pgcircuit_active_transactions", "gauge", "Active transactions")
	fmt.Fprintf(&b, "pgcircuit_active_transactions %d\n", runtime.ActiveTransactions)

	writePromHelp(&b, "pgcircuit_event_ring_warn", "gauge", "WARN decisions currently in the shared-memory event ring")
	fmt.Fprintf(&b, "pgcircuit_event_ring_warn %d\n", warnN)

	writePromHelp(&b, "pgcircuit_event_ring_block", "gauge", "BLOCK decisions currently in the shared-memory event ring")
	fmt.Fprintf(&b, "pgcircuit_event_ring_block %d\n", blockN)

	writePromHelp(&b, "pgcircuit_info", "gauge", "Extension metadata")
	fmt.Fprintf(&b, "pgcircuit_info{extension_version=%q,mode=%q,wal_pressure=%q,edition=%q} 1\n",
		status.ExtensionVersion, status.Mode, runtime.WalPressure, "community")

	return b.String(), nil
}

func writePromHelp(b *strings.Builder, name, typ, help string) {
	fmt.Fprintf(b, "# HELP %s %s\n# TYPE %s %s\n", name, help, name, typ)
}
