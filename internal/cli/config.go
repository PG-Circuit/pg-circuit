package cli

import (
	"context"
	"flag"
	"fmt"
	"io"
	"os"
	"strings"

	"github.com/jackc/pgx/v5"
)

const (
	ExitOK    = 0
	ExitUsage = 2
)

// Config holds CLI runtime options.
type Config struct {
	DSN    string
	Format string // text | json
}

// Client wraps a pgx connection.
type Client struct {
	Conn *pgx.Conn
}

// ParseFlags extracts global flags; remaining args are the command.
func ParseFlags(args []string) (Config, []string, error) {
	fs := flag.NewFlagSet("pgcircuit", flag.ContinueOnError)
	fs.SetOutput(io.Discard)

	dsnFlag := fs.String("dsn", "", "PostgreSQL connection string")
	format := fs.String("format", "text", "Output format: text or json")

	if err := fs.Parse(args); err != nil {
		if err == flag.ErrHelp {
			return Config{}, []string{"help"}, nil
		}
		return Config{}, nil, err
	}

	cfg := Config{
		DSN:    resolveDSN(*dsnFlag),
		Format: strings.ToLower(*format),
	}
	if cfg.Format != "text" && cfg.Format != "json" {
		return Config{}, nil, fmt.Errorf("invalid --format %q (want text or json)", cfg.Format)
	}
	return cfg, fs.Args(), nil
}

// resolveDSN precedence: --dsn > PGCIRCUIT_DSN > DATABASE_URL
func resolveDSN(flagDSN string) string {
	if strings.TrimSpace(flagDSN) != "" {
		return flagDSN
	}
	if v := os.Getenv("PGCIRCUIT_DSN"); strings.TrimSpace(v) != "" {
		return v
	}
	return os.Getenv("DATABASE_URL")
}

// ResolveDSNForTest exports resolveDSN for unit tests.
func ResolveDSNForTest(flagDSN string) string {
	return resolveDSN(flagDSN)
}

func PrintUsage(w io.Writer) {
	fmt.Fprintf(w, `pgcircuit — inspect PG Circuit Community from outside PostgreSQL

Usage:
  pgcircuit [--dsn DSN] [--format text|json] <command>

Commands:
  status     Extension and operating mode
  runtime    Runtime pressure and WAL signals (display-only in Community)
  blockers   Current lock blocker edges
  events     Recent WARN/BLOCK event history
  doctor     Connectivity checks + operator-loop tip
  version    CLI version

When blocked (operator loop):
  doctor → status → runtime → events
  SQL why: SELECT * FROM pg_circuit_explain_risk(...);

Connection (first match wins):
  --dsn
  PGCIRCUIT_DSN
  DATABASE_URL

`)
}

func Connect(ctx context.Context, cfg Config) (*Client, error) {
	if strings.TrimSpace(cfg.DSN) == "" {
		return nil, fmt.Errorf("no connection string (set --dsn, PGCIRCUIT_DSN, or DATABASE_URL)")
	}
	conn, err := pgx.Connect(ctx, cfg.DSN)
	if err != nil {
		return nil, fmt.Errorf("connect failed: %w", err)
	}
	return &Client{Conn: conn}, nil
}

func (c *Client) Close() {
	if c != nil && c.Conn != nil {
		_ = c.Conn.Close(context.Background())
	}
}

// SanitizeDSN redacts password for display.
func SanitizeDSN(dsn string) string {
	cfg, err := pgx.ParseConfig(dsn)
	if err != nil {
		return "[unparseable]"
	}
	host := cfg.Host
	if host == "" {
		host = "localhost"
	}
	user := cfg.User
	if user == "" {
		user = "?"
	}
	db := cfg.Database
	if db == "" {
		db = "?"
	}
	return fmt.Sprintf("postgres://%s@%s/%s", user, host, db)
}
